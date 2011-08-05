/* solver.c - Alpine Package Keeper (APK)
 * A backtracking, forward checking dependency graph solver.
 *
 * Copyright (C) 2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdlib.h>
#include "apk_defines.h"
#include "apk_database.h"
#include "apk_package.h"
#include "apk_solver.h"

#if 0
#include <stdio.h>
#define dbg_printf(args...) fprintf(stderr, args)
#else
#define dbg_printf(args...)
#endif

#define APK_PKGSTF_NOINSTALL		0
#define APK_PKGSTF_INSTALL		1
#define APK_PKGSTF_BRANCH		2
#define APK_PKGSTF_ALT_BRANCH		4

struct apk_package_state {
	struct apk_package *backtrack;
	unsigned int topology_depends;
	unsigned short flags;
	unsigned short conflicts;
	unsigned short cur_unsatisfiable;
};

#define APK_NAMESTF_AVAILABILITY_CHECKED	1
#define APK_NAMESTF_LOCKED			2
#define APK_NAMESTF_NO_OPTIONS			4
struct apk_name_state {
	struct list_head unsolved_list;
	struct apk_package *chosen;
	unsigned short flags;
	unsigned short requirers;
};

struct apk_solver_state {
	struct apk_database *db;
	struct apk_name_state *name_state;
	unsigned num_topology_positions;

	struct list_head unsolved_list_head;
	struct apk_package *latest_decision;
	unsigned int topology_position;
	unsigned int assigned_names;
	unsigned short cur_unsatisfiable;
	unsigned short allow_errors;

	struct apk_package_array *best_solution;
	unsigned short best_unsatisfiable;
};

static void apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static void undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static void push_decision(struct apk_solver_state *ss, struct apk_package *pkg,
			  int flags);

static inline struct apk_package_state *pkg_to_ps(struct apk_package *pkg)
{
	return (struct apk_package_state*) pkg->state_ptr;
}

static inline int pkg_available(struct apk_database *db, struct apk_package *pkg)
{
	if (pkg->installed_size == 0)
		return TRUE;
	if (pkg->filename != NULL)
		return TRUE;
	if (apk_db_select_repo(db, pkg) != NULL)
		return TRUE;
	return FALSE;
}

static void sort_pkg(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_package_state *ps;
	int i, j;

	/* Avoid recursion to same package */
	if (pkg->state_ptr != NULL)
		return;

	pkg->state_ptr = calloc(1, sizeof(struct apk_package_state));
	ps = pkg_to_ps(pkg);

	/* Sort all dependants before self */
	for (i = 0; i < pkg->depends->num; i++) {
		struct apk_dependency *dep = &pkg->depends->item[i];
		struct apk_name *name0 = dep->name;

		for (j = 0; j < name0->pkgs->num; j++) {
			struct apk_package *pkg0 = name0->pkgs->item[j];
			/* conflict depends on all to be not installed */
			if (dep->result_mask != APK_DEPMASK_CONFLICT &&
			    !apk_dep_is_satisfied(dep, pkg0))
				continue;
			sort_pkg(ss, pkg0);
		}
	}

	/* Assign a topology sorting order */
	ps->topology_depends = ++ss->num_topology_positions;

	/* FIXME: install_if */
}

static void sort_name(struct apk_solver_state *ss, struct apk_name *name)
{
	int i;

	for (i = 0; i < name->pkgs->num; i++)
		sort_pkg(ss, name->pkgs->item[i]);
}

static void prepare_name(struct apk_solver_state *ss, struct apk_name *name,
			 struct apk_name_state *ns)
{
	int i;

	if (ns->flags & APK_NAMESTF_AVAILABILITY_CHECKED)
		return;

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg = name->pkgs->item[i];
		struct apk_package_state *ps = pkg_to_ps(pkg);

		/* if package is needed for (re-)install */
		if ((name->flags & APK_NAME_REINSTALL) || (pkg->ipkg == NULL)) {
			/* and it's not available, we can't use it */
			if (!pkg_available(ss->db, pkg))
				ps->conflicts++;
		}
	}

	ns->flags |= APK_NAMESTF_AVAILABILITY_CHECKED;
}

static void foreach_dependency(struct apk_solver_state *ss, struct apk_dependency_array *deps,
			       void (*func)(struct apk_solver_state *ss, struct apk_dependency *dep))
{
	int i;
	for (i = 0; i < deps->num; i++)
		func(ss, &deps->item[i]);
}

static int inline can_consider_package(struct apk_solver_state *ss, struct apk_package_state *ps)
{
	if (ps == NULL)
		return FALSE;
	if (ps->topology_depends > ss->topology_position)
		return FALSE;
	if (ps->conflicts)
		return FALSE;
	return TRUE;
}

static int get_pkg_expansion_flags(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_name *name = pkg->name;
	int i, options = 0;

	/* check if the suggested package is the most preferred one of
	 * available packages for the name */
	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (pkg0 == pkg || !can_consider_package(ss, ps0))
			continue;

		if (apk_flags & APK_PREFER_AVAILABLE) {
			/* pkg available, pkg0 not */
			if (pkg->repos != 0 && pkg0->repos == 0)
				continue;
			/* pkg0 available, pkg not */
			if (pkg0->repos != 0 && pkg->repos == 0)
				return APK_PKGSTF_NOINSTALL | APK_PKGSTF_BRANCH;
		}

		if (!(apk_flags & APK_UPGRADE)) {
			/* not upgrading, prefer the installed package */
			if (pkg->ipkg == NULL && pkg0->ipkg != NULL)
				return APK_PKGSTF_NOINSTALL | APK_PKGSTF_BRANCH;
		}

		/* upgrading, or neither of the package is installed, so
		 * we just fall back comparing to versions */
		options++;
		if (apk_pkg_version_compare(pkg0, pkg) == APK_VERSION_GREATER)
			return APK_PKGSTF_NOINSTALL | APK_PKGSTF_BRANCH;
	}

	/* no package greater than the selected */
	if (options)
		return APK_PKGSTF_INSTALL | APK_PKGSTF_BRANCH;

	/* no other choice */
	return APK_PKGSTF_INSTALL;
}

static int update_name_state(struct apk_solver_state *ss,
			     struct apk_name *name, struct apk_name_state *ns,
			     int requirers_adjustment)
{
	struct apk_package *best_pkg = NULL;
	unsigned int best_topology = 0;
	int i, options = 0;

	ns->requirers += requirers_adjustment;

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (!can_consider_package(ss, ps0))
			continue;

		options++;
		if (best_pkg == NULL ||
		    ps0->topology_depends > best_topology)
			best_pkg = pkg0, best_topology = ps0->topology_depends;
	}

	if (options == 0) {
		if (!(ns->flags & APK_NAMESTF_NO_OPTIONS)) {
			ss->cur_unsatisfiable += ns->requirers;
			ns->flags |= APK_NAMESTF_NO_OPTIONS;
		} else if (requirers_adjustment > 0) {
			ss->cur_unsatisfiable += requirers_adjustment;
		}
	} else
		ns->flags &= ~APK_NAMESTF_NO_OPTIONS;

	if (options == 0 || ns->requirers == 0) {
		if (list_hashed(&ns->unsolved_list)) {
			list_del(&ns->unsolved_list);
			list_init(&ns->unsolved_list);
			ns->chosen = NULL;
		}
		dbg_printf("%s: deleted from unsolved: %d requirers, %d options\n",
			   name->name, ns->requirers, options);
	} else {
		dbg_printf("%s: added to unsolved: %d requirers, %d options (next topology %d)\n",
			   name->name, ns->requirers, options, best_topology);
		if (!list_hashed(&ns->unsolved_list))
			list_add(&ns->unsolved_list, &ss->unsolved_list_head);
		ns->chosen = best_pkg;
	}

	return options;
}

static void apply_decision(struct apk_solver_state *ss,
			   struct apk_package *pkg,
			   struct apk_package_state *ps)
{
	struct apk_name_state *ns = &ss->name_state[pkg->name->id];

	dbg_printf("apply_decision: " PKG_VER_FMT " %s\n", PKG_VER_PRINTF(pkg),
		   (ps->flags & APK_PKGSTF_INSTALL) ? "INSTALL" : "NO_INSTALL");

	if (ps->flags & APK_PKGSTF_INSTALL) {
		ss->assigned_names++;
		ns->chosen = pkg;
		ns->flags |= APK_NAMESTF_LOCKED;

		list_del(&ns->unsolved_list);
		list_init(&ns->unsolved_list);
		dbg_printf("%s: deleting from unsolved list\n",
			   pkg->name->name);

		foreach_dependency(ss, pkg->depends, apply_constraint);
	} else {
		ps->conflicts++;
		update_name_state(ss, pkg->name, ns, 0);
	}
}

static void undo_decision(struct apk_solver_state *ss,
			  struct apk_package *pkg,
			  struct apk_package_state *ps)
{
	struct apk_name_state *ns = &ss->name_state[pkg->name->id];

	dbg_printf("undo_decision: " PKG_VER_FMT " %s\n", PKG_VER_PRINTF(pkg),
		   (ps->flags & APK_PKGSTF_INSTALL) ? "INSTALL" : "NO_INSTALL");

	ss->cur_unsatisfiable = ps->cur_unsatisfiable;
	if (ps->flags & APK_PKGSTF_BRANCH)
		ss->topology_position = ps->topology_depends;

	if (ps->flags & APK_PKGSTF_INSTALL) {
		ss->assigned_names--;
		foreach_dependency(ss, pkg->depends, undo_constraint);

		ns->flags &= ~APK_NAMESTF_LOCKED;
		ns->chosen = NULL;
	} else {
		ps->conflicts--;
	}

	update_name_state(ss, pkg->name, ns, 0);
}

static void push_decision(struct apk_solver_state *ss, struct apk_package *pkg,
			  int flags)
{
	struct apk_package_state *ps = pkg_to_ps(pkg);

	ps->backtrack = ss->latest_decision;
	ps->flags = flags;
	ps->cur_unsatisfiable = ss->cur_unsatisfiable;
	ss->latest_decision = pkg;

	if (flags & APK_PKGSTF_BRANCH) {
		ss->topology_position = ps->topology_depends;
		dbg_printf("push_decision: adding new BRANCH at topology_position %d\n",
			   ss->topology_position);
	} else
		ps->flags |= APK_PKGSTF_ALT_BRANCH;

	apply_decision(ss, pkg, ps);
}

static int next_branch(struct apk_solver_state *ss)
{
	struct apk_package *pkg;
	struct apk_package_state *ps;

	while (ss->latest_decision != NULL) {
		pkg = ss->latest_decision;
		ps = pkg_to_ps(pkg);
		undo_decision(ss, pkg, ps);

		if (ps->flags & APK_PKGSTF_ALT_BRANCH) {
			pkg = ps->backtrack;
			ss->latest_decision = pkg;
			dbg_printf("next_branch: undo decision at topology_position %d\n",
				   ss->topology_position);
		} else {
			dbg_printf("next_branch: swapping BRANCH at topology_position %d\n",
				   ss->topology_position);

			ps->flags |= APK_PKGSTF_ALT_BRANCH;
			ps->flags ^= APK_PKGSTF_INSTALL;

			apply_decision(ss, pkg, ps);
			return 0;
		}
	}

	dbg_printf("next_branch: no more branches\n");
	return 1;
}

static void apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_state *ns = &ss->name_state[name->id];
	int i;

	prepare_name(ss, name, ns);

	if (ns->flags & APK_NAMESTF_LOCKED) {
		dbg_printf(PKG_VER_FMT " selected already for %s\n",
			   PKG_VER_PRINTF(ns->chosen), dep->name->name);
		if (!apk_dep_is_satisfied(dep, ns->chosen))
			ss->cur_unsatisfiable += 200;
		return;
	}

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0 == NULL ||
		    ps0->topology_depends >= ss->topology_position)
			continue;

		if (!apk_dep_is_satisfied(dep, pkg0)) {
			ps0->conflicts++;
			dbg_printf(PKG_VER_FMT ": conflicts++ -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
		}
	}

	update_name_state(ss, name, ns,
			  (dep->result_mask != APK_DEPMASK_CONFLICT) ? 1 : 0);
}

static void undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_state *ns = &ss->name_state[name->id];
	int i;

	if (ns->flags & APK_NAMESTF_LOCKED) {
		dbg_printf(PKG_VER_FMT " selected already for %s\n",
			   PKG_VER_PRINTF(ns->chosen), dep->name->name);
		return;
	}

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0->topology_depends >= ss->topology_position)
			continue;

		if (!apk_dep_is_satisfied(dep, pkg0)) {
			ps0->conflicts--;
			dbg_printf(PKG_VER_FMT ": conflicts-- -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
		}
	}

	update_name_state(ss, name, ns,
		(dep->result_mask != APK_DEPMASK_CONFLICT) ? -1 : 0);
}

static int expand_branch(struct apk_solver_state *ss)
{
	struct apk_name_state *ns;
	struct apk_package *pkg0 = NULL;
	unsigned int topology0 = 0;

	/* FIXME: change unsolved_list to a priority queue */
	list_for_each_entry(ns, &ss->unsolved_list_head, unsolved_list) {
		if (pkg0 == NULL ||
		    pkg_to_ps(ns->chosen)->topology_depends > topology0)
			pkg0 = ns->chosen, topology0 = pkg_to_ps(pkg0)->topology_depends;
	}
	if (pkg0 == NULL) {
		dbg_printf("expand_branch: list is empty (%d unsatisfied)\n",
			   ss->cur_unsatisfiable);
		return 1;
	}

	/* someone needs to provide this name -- find next eligible
	 * provider candidate */
	ns = &ss->name_state[pkg0->name->id];
	dbg_printf("expand_branch: %s\n", pkg0->name->name);

	push_decision(ss, pkg0, get_pkg_expansion_flags(ss, pkg0));
	#if 0
		      is_pkg_preferred(ss, pkg0) ?
			(APK_PKGSTF_INSTALL | APK_PKGSTF_BRANCH) :
			(APK_PKGSTF_NOINSTALL | APK_PKGSTF_BRANCH));
			#endif

	return 0;
}

static void record_solution(struct apk_solver_state *ss)
{
	struct apk_package *pkg;
	struct apk_package_state *ps;
	int i;

	apk_package_array_resize(&ss->best_solution, ss->assigned_names);

	i = 0;
	pkg = ss->latest_decision;
	while (pkg != NULL) {
		ps = pkg_to_ps(pkg);
		if ((ps->flags & APK_PKGSTF_INSTALL) &&
		    (ps->conflicts == 0))
			ss->best_solution->item[i++] = pkg;

		dbg_printf("record_solution: " PKG_VER_FMT ": %sINSTALL\n",
			   PKG_VER_PRINTF(pkg),
			   (ps->flags & APK_PKGSTF_INSTALL) ? "" : "NO_");

		pkg = ps->backtrack;
	}
	apk_package_array_resize(&ss->best_solution, i);
	ss->best_unsatisfiable = ss->cur_unsatisfiable;
}

static int compare_package_name(const void *p1, const void *p2)
{
	const struct apk_package **c1 = (const struct apk_package **) p1;
	const struct apk_package **c2 = (const struct apk_package **) p2;

	return strcmp((*c1)->name->name, (*c2)->name->name);
}

int apk_solver_solve(struct apk_database *db, struct apk_dependency_array *world,
		     struct apk_package_array **solution, int allow_errors)
{
	struct apk_solver_state *ss;
	int i, r;

	ss = calloc(1, sizeof(struct apk_solver_state));
	ss->db = db;
	ss->topology_position = -1;
	ss->best_unsatisfiable = -1;
	ss->allow_errors = allow_errors;
	list_init(&ss->unsolved_list_head);
	ss->name_state = calloc(db->available.names.num_items + 1, sizeof(struct apk_name_state));

	for (i = 0; i < world->num; i++)
		sort_name(ss, world->item[i].name);

	foreach_dependency(ss, world, apply_constraint);
	do {
		if (ss->allow_errors || ss->cur_unsatisfiable < ss->best_unsatisfiable) {
			r = expand_branch(ss);
			if (r) {
				if (ss->cur_unsatisfiable == 0) {
					/* found solution - it is optimal because we permutate
					 * each preferred local option first, and permutations
					 * happen in topologally sorted order. */
					r = 0;
					break;
				}
				if (ss->cur_unsatisfiable < ss->best_unsatisfiable)
					record_solution(ss);
				r = next_branch(ss);
			}
		} else {
			r = next_branch(ss);
		}
	} while (r == 0);

	/* collect packages */
	if (r == 0 && ss->cur_unsatisfiable == 0) {
		record_solution(ss);
		*solution = ss->best_solution;
		r = 0;
	} else if (ss->allow_errors) {
		*solution = ss->best_solution;
		qsort(ss->best_solution->item, ss->best_solution->num,
		      sizeof(struct apk_package *), compare_package_name);
		r = ss->best_unsatisfiable;
	} else
		r = -1;

	free(ss->name_state);
	free(ss);

	return r;
}

static int compare_change(const void *p1, const void *p2)
{
	const struct apk_change *c1 = (const struct apk_change *) p1;
	const struct apk_change *c2 = (const struct apk_change *) p2;

	if (c1->newpkg == NULL) {
		if (c2->newpkg == NULL)
			/* both deleted - reverse topology order */
			return  pkg_to_ps(c1->oldpkg)->topology_depends -
				pkg_to_ps(c2->oldpkg)->topology_depends;

		/* c1 deleted, c2 installed -> c2 first*/
		return -1;
	}
	if (c2->newpkg == NULL)
		/* c1 installed, c2 deleted -> c1 first*/
		return 1;

	return  pkg_to_ps(c2->newpkg)->topology_depends -
		pkg_to_ps(c1->newpkg)->topology_depends;
}

int apk_solver_generate_changeset(struct apk_database *db,
				  struct apk_package_array *solution,
				  struct apk_changeset *changeset)
{
	struct apk_name *name;
	struct apk_package *pkg, *pkg0;
	struct apk_installed_package *ipkg;
	int num_installs = 0, num_removed = 0, ci = 0;
	int i, j;

	/* calculate change set size */
	for (i = 0; i < solution->num; i++) {
		pkg = solution->item[i];
		name = pkg->name;
		if (pkg->ipkg == NULL || (name->flags & APK_NAME_REINSTALL))
			num_installs++;
		name->flags |= APK_NAME_VISITED;
	}

	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		if (!(ipkg->pkg->name->flags & APK_NAME_VISITED))
			num_removed++;
	}

	/* construct changeset */
	apk_change_array_resize(&changeset->changes, num_installs + num_removed);
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		if (ipkg->pkg->name->flags & APK_NAME_VISITED)
			continue;
		changeset->changes->item[ci].oldpkg = ipkg->pkg;
		ci++;
	}
	for (i = 0; i < solution->num; i++) {
		pkg = solution->item[i];
		name = pkg->name;

		if (pkg->ipkg == NULL || (name->flags & APK_NAME_REINSTALL)) {
			for (j = 0; j < name->pkgs->num; j++) {
				pkg0 = name->pkgs->item[j];
				if (pkg0->ipkg == NULL)
					continue;
				changeset->changes->item[ci].oldpkg = pkg0;
				break;
			}
			changeset->changes->item[ci].newpkg = pkg;
			ci++;
		}
		name->flags &= ~APK_NAME_VISITED;
	}

	/* sort changeset to topology order */
	qsort(changeset->changes->item, changeset->changes->num,
	      sizeof(struct apk_change), compare_change);

	return 0;
}
