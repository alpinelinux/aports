/* solver.c - Alpine Package Keeper (APK)
 * A backtracking, forward checking dependency graph solver.
 *
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
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

#include "apk_print.h"

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
#define APK_PKGSTF_INSTALLIF		8
#define APK_PKGSTF_DECIDED		16

struct apk_package_state {
	struct apk_package *backtrack;
	unsigned int topology_hard, topology_soft;
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
	unsigned short solver_flags;
	unsigned short flags;
	unsigned short requirers;
	unsigned short install_ifs;
};

struct apk_solver_state {
	struct apk_database *db;
	unsigned num_topology_positions;

	struct list_head unsolved_list_head;
	struct apk_package *latest_decision;
	unsigned int topology_position;
	unsigned int assigned_names;
	unsigned short solver_flags;
	unsigned short cur_unsatisfiable;

	struct apk_package_array *best_solution;
	unsigned short best_unsatisfiable;
};

static void apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static void undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static void push_decision(struct apk_solver_state *ss, struct apk_package *pkg,
			  int flags);

static struct apk_package_state *pkg_to_ps(struct apk_package *pkg)
{
	return (struct apk_package_state*) pkg->state_ptr;
}

static struct apk_name_state *name_to_ns(struct apk_name *name)
{
	if (name->state_ptr == NULL)
		name->state_ptr = calloc(1, sizeof(struct apk_name_state));
	return (struct apk_name_state*) name->state_ptr;
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

static void foreach_dependency_pkg(
	struct apk_solver_state *ss, struct apk_dependency_array *depends,
	void (*cb)(struct apk_solver_state *ss, struct apk_package *dependency))
{
	int i, j;

	/* And sort the main dependencies */
	for (i = 0; i < depends->num; i++) {
		struct apk_dependency *dep = &depends->item[i];
		struct apk_name *name0 = dep->name;

		for (j = 0; j < name0->pkgs->num; j++) {
			struct apk_package *pkg0 = name0->pkgs->item[j];

			/* conflict depends on all to be not installed */
			if (dep->result_mask != APK_DEPMASK_CONFLICT &&
			    !apk_dep_is_satisfied(dep, pkg0))
				continue;

			cb(ss, pkg0);
		}
	}
}

static void foreach_rinstall_if_pkg(
	struct apk_solver_state *ss, struct apk_package *pkg,
	void (*cb)(struct apk_solver_state *ss, struct apk_package *rinstall_if))
{
	struct apk_name *name = pkg->name;
	int i, j, k;

	for (i = 0; i < pkg->name->rinstall_if->num; i++) {
		struct apk_name *name0 = pkg->name->rinstall_if->item[i];

		dbg_printf(PKG_VER_FMT ": rinstall_if %s\n",
			   PKG_VER_PRINTF(pkg), name0->name);

		for (j = 0; j < name0->pkgs->num; j++) {
			struct apk_package *pkg0 = name0->pkgs->item[j];

			for (k = 0; k < pkg0->install_if->num; k++) {
				struct apk_dependency *dep = &pkg0->install_if->item[k];
				if (dep->name == name &&
				    (dep->result_mask == APK_DEPMASK_CONFLICT ||
				     apk_dep_is_satisfied(dep, pkg)))
					break;
			}
			if (k >= pkg0->install_if->num)
				continue;

			/* pkg depends (via install_if) on pkg0 */
			cb(ss, pkg0);
		}
	}
}

static void sort_hard_dependencies(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_package_state *ps;

	if (pkg->state_ptr == NULL)
		pkg->state_ptr = calloc(1, sizeof(struct apk_package_state));

	ps = pkg_to_ps(pkg);
	if (ps->topology_hard)
		return;
	ps->topology_hard = -1;

	/* Consider hard dependencies only */
	foreach_dependency_pkg(ss, pkg->depends, sort_hard_dependencies);
	foreach_dependency_pkg(ss, pkg->install_if, sort_hard_dependencies);

	ps->topology_soft = ps->topology_hard = ++ss->num_topology_positions;
	dbg_printf(PKG_VER_FMT ": topology_hard=%d\n",
		   PKG_VER_PRINTF(pkg), ps->topology_hard);
}

static void sort_soft_dependencies(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_package_state *ps;

	sort_hard_dependencies(ss, pkg);

	ps = pkg_to_ps(pkg);
	if (ps->topology_soft != ps->topology_hard)
		return;
	ps->topology_soft = -1;

	/* Soft reverse dependencies aka. install_if */
	foreach_rinstall_if_pkg(ss, pkg, sort_hard_dependencies);
	foreach_dependency_pkg(ss, pkg->depends, sort_soft_dependencies);

	/* Assign a topology sorting order */
	ps->topology_soft = ++ss->num_topology_positions;
	dbg_printf(PKG_VER_FMT ": topology_soft=%d\n",
		   PKG_VER_PRINTF(pkg), ps->topology_soft);
}

static void sort_name(struct apk_solver_state *ss, struct apk_name *name)
{
	int i;

	for (i = 0; i < name->pkgs->num; i++)
		sort_soft_dependencies(ss, name->pkgs->item[i]);
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
		struct apk_name_state *ns = name_to_ns(pkg->name);

		/* if package is needed for (re-)install */
		if ((pkg->ipkg == NULL) ||
		    ((ns->solver_flags | ss->solver_flags) & APK_SOLVERF_REINSTALL)) {
			/* and it's not available, we can't use it */
			if (!pkg_available(ss->db, pkg))
				ps->conflicts = 1024;
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

static int get_pkg_expansion_flags(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_name *name = pkg->name;
	struct apk_name_state *ns = name_to_ns(name);
	int i, options = 0;

	/* check if the suggested package is the most preferred one of
	 * available packages for the name */
	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (pkg0 == pkg || ps0 == NULL ||
		    ps0->topology_hard > ss->topology_position ||
		    (ps0->flags & APK_PKGSTF_DECIDED) ||
		    ps0->conflicts != 0)
			continue;

		options++;

		if ((ns->solver_flags | ss->solver_flags) & APK_SOLVERF_AVAILABLE) {
			/* pkg available, pkg0 not */
			if (pkg->repos != 0 && pkg0->repos == 0)
				continue;
			/* pkg0 available, pkg not */
			if (pkg0->repos != 0 && pkg->repos == 0)
				return APK_PKGSTF_NOINSTALL | APK_PKGSTF_BRANCH;
		}

		if ((ns->solver_flags | ss->solver_flags) & APK_SOLVERF_UPGRADE) {
			/* upgrading, or neither of the package is installed, so
			 * we just fall back comparing to versions */
			switch (apk_pkg_version_compare(pkg0, pkg)) {
			case APK_VERSION_GREATER:
				return APK_PKGSTF_NOINSTALL | APK_PKGSTF_BRANCH;
			case APK_VERSION_LESS:
				continue;
			}
		}

		/* not upgrading, prefer the installed package */
		if (pkg->ipkg == NULL && pkg0->ipkg != NULL)
			return APK_PKGSTF_NOINSTALL | APK_PKGSTF_BRANCH;
	}

	/* no package greater than the selected */
	if (options)
		return APK_PKGSTF_INSTALL | APK_PKGSTF_BRANCH;

	/* no other choice */
	return APK_PKGSTF_INSTALL;
}

static int install_if_missing(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_name_state *ns;
	int i, missing = 0;

	for (i = 0; i < pkg->install_if->num; i++) {
		struct apk_dependency *dep = &pkg->install_if->item[i];

		ns = name_to_ns(dep->name);
		if (!(ns->flags & APK_NAMESTF_LOCKED) ||
		    !apk_dep_is_satisfied(dep, ns->chosen))
			missing++;
	}

	return missing;
}

static int update_name_state(struct apk_solver_state *ss,
			     struct apk_name *name, struct apk_name_state *ns,
			     int requirers_adjustment)
{
	struct apk_package *best_pkg = NULL;
	unsigned int best_topology = 0;
	int i, options = 0, skipped_options = 0;

	ns->requirers += requirers_adjustment;

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0 == NULL ||
		    ps0->topology_hard >= ss->topology_position ||
		    (ps0->flags & APK_PKGSTF_DECIDED))
			continue;

		if (ns->requirers == 0 && ns->install_ifs != 0 &&
		    install_if_missing(ss, pkg0)) {
			skipped_options++;
			continue;
		}

		options++;
		if (ps0->topology_soft < ss->topology_position &&
		    ps0->topology_soft > best_topology)
			best_pkg = pkg0, best_topology = ps0->topology_soft;
		else if (ps0->topology_hard > best_topology)
			best_pkg = pkg0, best_topology = ps0->topology_hard;
	}

	if (options == 0 && skipped_options == 0) {
		if (!(ns->flags & APK_NAMESTF_NO_OPTIONS)) {
			ss->cur_unsatisfiable += ns->requirers;
			if (ns->install_ifs)
				ss->cur_unsatisfiable++;
			ns->flags |= APK_NAMESTF_NO_OPTIONS;
		} else if (requirers_adjustment > 0) {
			ss->cur_unsatisfiable += requirers_adjustment;
		}
	} else
		ns->flags &= ~APK_NAMESTF_NO_OPTIONS;

	if ((options == 0 && skipped_options == 0) ||
	    (ns->requirers == 0 && ns->install_ifs == 0)) {
		if (list_hashed(&ns->unsolved_list)) {
			list_del(&ns->unsolved_list);
			list_init(&ns->unsolved_list);
			ns->chosen = NULL;
		}
		dbg_printf("%s: deleted from unsolved: %d requirers, %d install_ifs, %d options, %d skipped\n",
			   name->name, ns->requirers, ns->install_ifs, options, skipped_options);
	} else {
		dbg_printf("%s: added to unsolved: %d requirers, %d install_ifs, %d options (next topology %d)\n",
			   name->name, ns->requirers, ns->install_ifs, options,
			   best_topology);
		if (!list_hashed(&ns->unsolved_list))
			list_add(&ns->unsolved_list, &ss->unsolved_list_head);
		ns->chosen = best_pkg;
	}

	return options + skipped_options;
}

static void trigger_install_if(struct apk_solver_state *ss,
			       struct apk_package *pkg)
{
	if (install_if_missing(ss, pkg) == 0) {
		struct apk_name_state *ns = ns = name_to_ns(pkg->name);

		dbg_printf("trigger_install_if: " PKG_VER_FMT " triggered\n",
			   PKG_VER_PRINTF(pkg));
		ns->install_ifs++;
		update_name_state(ss, pkg->name, ns, 0);
	}
}

static void untrigger_install_if(struct apk_solver_state *ss,
			       struct apk_package *pkg)
{
	if (install_if_missing(ss, pkg) != 1) {
		struct apk_name_state *ns = name_to_ns(pkg->name);

		dbg_printf("untrigger_install_if: " PKG_VER_FMT " no longer triggered\n",
			   PKG_VER_PRINTF(pkg));
		ns->install_ifs--;
		update_name_state(ss, pkg->name, ns, 0);
	}
}

static void apply_decision(struct apk_solver_state *ss,
			   struct apk_package *pkg,
			   struct apk_package_state *ps)
{
	struct apk_name_state *ns = name_to_ns(pkg->name);

	dbg_printf("apply_decision: " PKG_VER_FMT " %s\n", PKG_VER_PRINTF(pkg),
		   (ps->flags & APK_PKGSTF_INSTALL) ? "INSTALL" : "NO_INSTALL");

	if (ps->flags & APK_PKGSTF_INSTALL) {
		ss->assigned_names++;
		ss->cur_unsatisfiable += ps->conflicts;
		ns->chosen = pkg;
		ns->flags |= APK_NAMESTF_LOCKED;

		list_del(&ns->unsolved_list);
		list_init(&ns->unsolved_list);
		dbg_printf("%s: deleting from unsolved list\n",
			   pkg->name->name);

		foreach_dependency(ss, pkg->depends, apply_constraint);
		foreach_rinstall_if_pkg(ss, pkg, trigger_install_if);
	} else {
		update_name_state(ss, pkg->name, ns, 0);
	}
}

static void undo_decision(struct apk_solver_state *ss,
			  struct apk_package *pkg,
			  struct apk_package_state *ps)
{
	struct apk_name_state *ns = name_to_ns(pkg->name);

	dbg_printf("undo_decision: " PKG_VER_FMT " %s\n", PKG_VER_PRINTF(pkg),
		   (ps->flags & APK_PKGSTF_INSTALL) ? "INSTALL" : "NO_INSTALL");

	if (ps->flags & APK_PKGSTF_INSTALLIF)
		ss->topology_position = ps->topology_soft;
	else
		ss->topology_position = ps->topology_hard;

	if (ps->flags & APK_PKGSTF_INSTALL) {
		ss->assigned_names--;

		foreach_rinstall_if_pkg(ss, pkg, untrigger_install_if);
		foreach_dependency(ss, pkg->depends, undo_constraint);

		ns->flags &= ~APK_NAMESTF_LOCKED;
		ns->chosen = NULL;
	}

	ss->cur_unsatisfiable = ps->cur_unsatisfiable;
	update_name_state(ss, pkg->name, ns, 0);
}

static void push_decision(struct apk_solver_state *ss, struct apk_package *pkg,
			  int flags)
{
	struct apk_package_state *ps = pkg_to_ps(pkg);

	ps->backtrack = ss->latest_decision;
	ps->flags = flags | APK_PKGSTF_DECIDED;
	ps->cur_unsatisfiable = ss->cur_unsatisfiable;

	if (ps->topology_soft < ss->topology_position) {
		if (flags & APK_PKGSTF_INSTALL)
			ps->flags |= APK_PKGSTF_INSTALLIF;
		ss->topology_position = ps->topology_soft;
	} else {
		ps->flags &= ~APK_PKGSTF_INSTALLIF;
		ss->topology_position = ps->topology_hard;
	}
	ss->latest_decision = pkg;

	if (flags & APK_PKGSTF_BRANCH) {
		dbg_printf("push_decision: adding new BRANCH at topology_position %d\n",
			   ss->topology_position);
	} else
		ps->flags |= APK_PKGSTF_ALT_BRANCH;

	if (ps->flags & APK_PKGSTF_INSTALLIF)
		dbg_printf("triggers due to " PKG_VER_FMT "\n",
			   PKG_VER_PRINTF(pkg));

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
			dbg_printf("next_branch: undo decision at topology_position %d\n",
				   ss->topology_position);
			ps->flags &= ~(APK_PKGSTF_ALT_BRANCH | APK_PKGSTF_DECIDED);
			ss->latest_decision = ps->backtrack;
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
	struct apk_name_state *ns = name_to_ns(name);
	int i;

	prepare_name(ss, name, ns);

	if (ns->flags & APK_NAMESTF_LOCKED) {
		dbg_printf(PKG_VER_FMT " selected already for %s\n",
			   PKG_VER_PRINTF(ns->chosen), dep->name->name);
		if (!apk_dep_is_satisfied(dep, ns->chosen))
			ss->cur_unsatisfiable++;
		return;
	}

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0 == NULL ||
		    ps0->topology_hard >= ss->topology_position)
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
	struct apk_name_state *ns = name_to_ns(name);
	int i;

	if (ns->flags & APK_NAMESTF_LOCKED) {
		dbg_printf(PKG_VER_FMT " selected already for %s\n",
			   PKG_VER_PRINTF(ns->chosen), dep->name->name);
		return;
	}

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0->topology_hard >= ss->topology_position)
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
		/* ns->chosen can be NULL if the name has only install_if
		 * requirers that got later conflicted, but it still has
		 * other options that can get activated later due to more
		 * complicated install_if rules in some other package. */
		if (ns->chosen == NULL)
			continue;
		if (pkg_to_ps(ns->chosen)->topology_soft < ss->topology_position &&
		    pkg_to_ps(ns->chosen)->topology_soft > topology0)
			pkg0 = ns->chosen, topology0 = pkg_to_ps(pkg0)->topology_soft;
		else if (pkg_to_ps(ns->chosen)->topology_hard > topology0)
			pkg0 = ns->chosen, topology0 = pkg_to_ps(pkg0)->topology_hard;
	}
	if (pkg0 == NULL) {
		dbg_printf("expand_branch: list is empty (%d unsatisfied)\n",
			   ss->cur_unsatisfiable);
		return 1;
	}

	/* someone needs to provide this name -- find next eligible
	 * provider candidate */
	ns = name_to_ns(pkg0->name);
	dbg_printf("expand_branch: %s\n", pkg0->name->name);

	push_decision(ss, pkg0, get_pkg_expansion_flags(ss, pkg0));

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
		if (ps->flags & APK_PKGSTF_INSTALL) {
			if (i >= ss->assigned_names)
				abort();
			ss->best_solution->item[i++] = pkg;
		}

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

static int compare_change(const void *p1, const void *p2)
{
	const struct apk_change *c1 = (const struct apk_change *) p1;
	const struct apk_change *c2 = (const struct apk_change *) p2;

	if (c1->newpkg == NULL) {
		if (c2->newpkg == NULL)
			/* both deleted - reverse topology order */
			return  pkg_to_ps(c2->oldpkg)->topology_hard -
				pkg_to_ps(c1->oldpkg)->topology_hard;

		/* c1 deleted, c2 installed -> c2 first*/
		return -1;
	}
	if (c2->newpkg == NULL)
		/* c1 installed, c2 deleted -> c1 first*/
		return 1;

	return  pkg_to_ps(c1->newpkg)->topology_hard -
		pkg_to_ps(c2->newpkg)->topology_hard;
}

static int generate_changeset(struct apk_database *db,
			      struct apk_package_array *solution,
			      struct apk_changeset *changeset,
			      unsigned short solver_flags)
{
	struct apk_name *name;
	struct apk_name_state *ns;
	struct apk_package *pkg, *pkg0;
	struct apk_installed_package *ipkg;
	int i, j, num_installs = 0, num_removed = 0, ci = 0;

	/* calculate change set size */
	for (i = 0; i < solution->num; i++) {
		pkg = solution->item[i];
		if ((pkg->ipkg == NULL) ||
		    ((solver_flags | name_to_ns(pkg->name)->solver_flags) & APK_SOLVERF_REINSTALL))
			num_installs++;
	}
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		name = ipkg->pkg->name;
		ns = name_to_ns(name);
		if ((ns->chosen == NULL) || !(ns->flags & APK_NAMESTF_LOCKED))
			num_removed++;
	}

	/* construct changeset */
	apk_change_array_resize(&changeset->changes, num_installs + num_removed);
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		name = ipkg->pkg->name;
		ns = name_to_ns(name);
		if ((ns->chosen == NULL) || !(ns->flags & APK_NAMESTF_LOCKED)) {
			changeset->changes->item[ci].oldpkg = ipkg->pkg;
			ci++;
		}
	}
	for (i = 0; i < solution->num; i++) {
		pkg = solution->item[i];
		name = pkg->name;

		if ((pkg->ipkg == NULL) ||
		    ((solver_flags | name_to_ns(name)->solver_flags) & APK_SOLVERF_REINSTALL)) {
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
	}

	/* sort changeset to topology order */
	qsort(changeset->changes->item, changeset->changes->num,
	      sizeof(struct apk_change), compare_change);

	return 0;
}

static int free_state(apk_hash_item item, void *ctx)
{
	struct apk_name *name = (struct apk_name *) item;

	if (name->state_ptr != NULL) {
		free(name->state_ptr);
		name->state_ptr = NULL;
	}
	return 0;
}

static int free_package(apk_hash_item item, void *ctx)
{
	struct apk_package *pkg = (struct apk_package *) item;

	if (pkg->state_ptr != NULL) {
		free(pkg->state_ptr);
		pkg->state_ptr = NULL;
	}
	return 0;
}

void apk_solver_set_name_flags(struct apk_name *name,
			       unsigned short solver_flags)
{
	struct apk_name_state *ns = name_to_ns(name);
	ns->solver_flags = solver_flags;
}

int apk_solver_solve(struct apk_database *db,
		     unsigned short solver_flags,
		     struct apk_dependency_array *world,
		     struct apk_package_array **solution,
		     struct apk_changeset *changeset)
{
	struct apk_solver_state *ss;
	struct apk_installed_package *ipkg;
	int i, r;

	ss = calloc(1, sizeof(struct apk_solver_state));
	ss->db = db;
	ss->solver_flags = solver_flags;
	ss->topology_position = -1;
	ss->best_unsatisfiable = -1;
	list_init(&ss->unsolved_list_head);

	for (i = 0; i < world->num; i++)
		sort_name(ss, world->item[i].name);
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list)
		sort_name(ss, ipkg->pkg->name);

	foreach_dependency(ss, world, apply_constraint);
	do {
		if (ss->cur_unsatisfiable < ss->best_unsatisfiable) {
			r = expand_branch(ss);
			if (r) {
				dbg_printf("solution with %d unsatisfiable\n",
					   ss->cur_unsatisfiable);
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
		if (changeset != NULL)
			generate_changeset(db, ss->best_solution, changeset,
					   ss->solver_flags);
		r = 0;
	} else {
		qsort(ss->best_solution->item, ss->best_solution->num,
		      sizeof(struct apk_package *), compare_package_name);
		r = ss->best_unsatisfiable;
	}

	if (solution != NULL)
		*solution = ss->best_solution;
	else
		apk_package_array_free(&ss->best_solution);

	if (!(solver_flags & APK_SOLVERF_KEEP_STATE))
		apk_solver_free(db);

	free(ss);

	return r;
}

void apk_solver_free(struct apk_database *db)
{
	apk_hash_foreach(&db->available.names, free_state, NULL);
	apk_hash_foreach(&db->available.packages, free_package, NULL);
}

static void print_change(struct apk_database *db,
			 struct apk_change *change,
			 int cur, int total)
{
	struct apk_name *name;
	struct apk_package *oldpkg = change->oldpkg;
	struct apk_package *newpkg = change->newpkg;
	const char *msg = NULL;
	char status[64];
	int r;

	snprintf(status, sizeof(status), "(%i/%i)", cur+1, total);
	status[sizeof(status) - 1] = '\0';

	if (oldpkg != NULL)
		name = oldpkg->name;
	else
		name = newpkg->name;

	if (oldpkg == NULL) {
		apk_message("%s Installing %s (" BLOB_FMT ")",
			    status, name->name,
			    BLOB_PRINTF(*newpkg->version));
	} else if (newpkg == NULL) {
		apk_message("%s Purging %s (" BLOB_FMT ")",
			    status, name->name,
			    BLOB_PRINTF(*oldpkg->version));
	} else {
		r = apk_pkg_version_compare(newpkg, oldpkg);
		switch (r) {
		case APK_VERSION_LESS:
			msg = "Downgrading";
			break;
		case APK_VERSION_EQUAL:
			if (newpkg == oldpkg)
				msg = "Re-installing";
			else
				msg = "Replacing";
			break;
		case APK_VERSION_GREATER:
			msg = "Upgrading";
			break;
		}
		apk_message("%s %s %s (" BLOB_FMT " -> " BLOB_FMT ")",
			    status, msg, name->name,
			    BLOB_PRINTF(*oldpkg->version),
			    BLOB_PRINTF(*newpkg->version));
	}
}

struct apk_stats {
	unsigned int bytes;
	unsigned int packages;
};

static void count_change(struct apk_change *change, struct apk_stats *stats)
{
	if (change->newpkg != NULL) {
		stats->bytes += change->newpkg->installed_size;
		stats->packages ++;
	}
	if (change->oldpkg != NULL)
		stats->packages ++;
}

static void draw_progress(int percent)
{
	const int bar_width = apk_get_screen_width() - 7;
	int i;

	fprintf(stderr, "\e7%3i%% [", percent);
	for (i = 0; i < bar_width * percent / 100; i++)
		fputc('#', stderr);
	for (; i < bar_width; i++)
		fputc(' ', stderr);
	fputc(']', stderr);
	fflush(stderr);
	fputs("\e8\e[0K", stderr);
}

struct progress {
	struct apk_stats done;
	struct apk_stats total;
	struct apk_package *pkg;
	size_t count;
};

static void progress_cb(void *ctx, size_t progress)
{
	struct progress *prog = (struct progress *) ctx;
	size_t partial = 0, count;

	if (prog->pkg != NULL)
		partial = muldiv(progress, prog->pkg->installed_size, APK_PROGRESS_SCALE);

        count = muldiv(100, prog->done.bytes + prog->done.packages + partial,
		       prog->total.bytes + prog->total.packages);

	if (prog->count != count)
		draw_progress(count);
	prog->count = count;
}

static int dump_packages(struct apk_changeset *changeset,
			 int (*cmp)(struct apk_change *change),
			 const char *msg)
{
	struct apk_change *change;
	struct apk_name *name;
	struct apk_indent indent = { .indent = 2 };
	int match = 0, i;

	for (i = 0; i < changeset->changes->num; i++) {
		change = &changeset->changes->item[i];
		if (!cmp(change))
			continue;
		if (match == 0)
			printf("%s:\n ", msg);
		if (change->newpkg != NULL)
			name = change->newpkg->name;
		else
			name = change->oldpkg->name;

		apk_print_indented(&indent, APK_BLOB_STR(name->name));
		match++;
	}
	if (match)
		printf("\n");
	return match;
}

static int cmp_remove(struct apk_change *change)
{
	return change->newpkg == NULL;
}

static int cmp_new(struct apk_change *change)
{
	return change->oldpkg == NULL;
}

static int cmp_downgrade(struct apk_change *change)
{
	if (change->newpkg == NULL || change->oldpkg == NULL)
		return 0;
	if (apk_pkg_version_compare(change->newpkg, change->oldpkg)
	    & APK_VERSION_LESS)
		return 1;
	return 0;
}

static int cmp_upgrade(struct apk_change *change)
{
	if (change->newpkg == NULL || change->oldpkg == NULL)
		return 0;

	/* Count swapping package as upgrade too - this can happen if
	 * same package version is used after it was rebuilt against
	 * newer libraries. Basically, different (and probably newer)
	 * package, but equal version number. */
	if ((apk_pkg_version_compare(change->newpkg, change->oldpkg) &
	     (APK_VERSION_GREATER | APK_VERSION_EQUAL)) &&
	    (change->newpkg != change->oldpkg))
		return 1;

	return 0;
}

static int compare_package(const void *p1, const void *p2)
{
	struct apk_package *pkg1 = *(struct apk_package **) p1;
	struct apk_package *pkg2 = *(struct apk_package **) p2;

	return pkg_to_ps(pkg1)->topology_hard - pkg_to_ps(pkg2)->topology_hard;
}

static void run_triggers(struct apk_database *db)
{
	struct apk_package_array *pkgs;
	int i;

	pkgs = apk_db_get_pending_triggers(db);
	if (pkgs == NULL || pkgs->num == 0)
		return;

	qsort(pkgs->item, pkgs->num, sizeof(struct apk_package *),
	      compare_package);

	for (i = 0; i < pkgs->num; i++) {
		struct apk_package *pkg = pkgs->item[i];
		struct apk_installed_package *ipkg = pkg->ipkg;

		*apk_string_array_add(&ipkg->pending_triggers) = NULL;
		apk_ipkg_run_script(ipkg, db, APK_SCRIPT_TRIGGER,
				    ipkg->pending_triggers->item);
		apk_string_array_free(&ipkg->pending_triggers);
	}
	apk_package_array_free(&pkgs);
}

int apk_solver_commit_changeset(struct apk_database *db,
				struct apk_changeset *changeset,
				struct apk_dependency_array *world)
{
	struct progress prog;
	struct apk_change *change;
	int i, r = 0, size_diff = 0;

	if (changeset->changes == NULL)
		goto all_done;

	/* Count what needs to be done */
	memset(&prog, 0, sizeof(prog));
	for (i = 0; i < changeset->changes->num; i++) {
		change = &changeset->changes->item[i];
		count_change(change, &prog.total);
		if (change->newpkg)
			size_diff += change->newpkg->installed_size;
		if (change->oldpkg)
			size_diff -= change->oldpkg->installed_size;
	}
	size_diff /= 1024;

	if (apk_verbosity > 1 || (apk_flags & APK_INTERACTIVE)) {
		r = dump_packages(changeset, cmp_remove,
				  "The following packages will be REMOVED");
		r += dump_packages(changeset, cmp_downgrade,
				   "The following packages will be DOWNGRADED");
		if (r || (apk_flags & APK_INTERACTIVE) || apk_verbosity > 2) {
			dump_packages(changeset, cmp_new,
				      "The following NEW packages will be installed");
			dump_packages(changeset, cmp_upgrade,
				      "The following packages will be upgraded");
			printf("After this operation, %d kB of %s\n", abs(size_diff),
			       (size_diff < 0) ?
			       "disk space will be freed." :
			       "additional disk space will be used.");
		}
		if (apk_flags & APK_INTERACTIVE) {
			printf("Do you want to continue [Y/n]? ");
			fflush(stdout);
			r = fgetc(stdin);
			if (r != 'y' && r != 'Y' && r != '\n')
				return -1;
		}
	}

	/* Go through changes */
	r = 0;
	for (i = 0; i < changeset->changes->num; i++) {
		change = &changeset->changes->item[i];

		print_change(db, change, i, changeset->changes->num);
		if (apk_flags & APK_PROGRESS)
			draw_progress(prog.count);
		prog.pkg = change->newpkg;

		if (!(apk_flags & APK_SIMULATE)) {
			r = apk_db_install_pkg(db,
					       change->oldpkg, change->newpkg,
					       (apk_flags & APK_PROGRESS) ? progress_cb : NULL,
					       &prog);
			if (r != 0)
				break;
		}

		count_change(change, &prog.done);
	}
	if (apk_flags & APK_PROGRESS)
		draw_progress(100);

	run_triggers(db);

all_done:
	apk_dependency_array_copy(&db->world, world);
	apk_db_write_config(db);

	if (r == 0 && !db->performing_self_update) {
		apk_message("OK: %d packages, %d dirs, %d files",
			    db->installed.stats.packages,
			    db->installed.stats.dirs,
			    db->installed.stats.files);
	}

	return r;
}

static void print_dep_errors(char *label, struct apk_dependency_array *deps)
{
	int i, print_label = 1;
	char buf[256];
	apk_blob_t p;
	struct apk_indent indent;

	for (i = 0; i < deps->num; i++) {
		struct apk_dependency *dep = &deps->item[i];
		struct apk_package *pkg = (struct apk_package*) dep->name->state_ptr;

		if (pkg != NULL && apk_dep_is_satisfied(dep, pkg))
			continue;

		if (print_label) {
			print_label = 0;
			indent.x = printf("  %s:", label);
			indent.indent = indent.x + 1;
		}
		p = APK_BLOB_BUF(buf);
		apk_blob_push_dep(&p, dep);
		p = apk_blob_pushed(APK_BLOB_BUF(buf), p);
		apk_print_indented(&indent, p);
	}
	if (!print_label)
		printf("\n");
}

void apk_solver_print_errors(struct apk_database *db,
			     struct apk_package_array *solution,
			     struct apk_dependency_array *world,
			     int unsatisfiable)
{
	int i;

	apk_error("%d unsatisfiable dependencies:", unsatisfiable);

	for (i = 0; i < solution->num; i++) {
		struct apk_package *pkg = solution->item[i];
		pkg->name->state_ptr = pkg;
	}

	print_dep_errors("world", world);
	for (i = 0; i < solution->num; i++) {
		struct apk_package *pkg = solution->item[i];
		char pkgtext[256];
		snprintf(pkgtext, sizeof(pkgtext), PKG_VER_FMT, PKG_VER_PRINTF(solution->item[i]));
		print_dep_errors(pkgtext, pkg->depends);
	}
}

int apk_solver_commit(struct apk_database *db,
		      unsigned short solver_flags,
		      struct apk_dependency_array *world)
{
	struct apk_changeset changeset = {};
	struct apk_package_array *solution = NULL;
	int r;

	r = apk_solver_solve(db, APK_SOLVERF_KEEP_STATE | solver_flags, 
			     world, &solution, &changeset);
	if (r < 0)
		return r;

	if (r == 0 || (apk_flags & APK_FORCE)) {
		/* Success -- or forced installation of bad graph */
		apk_solver_commit_changeset(db, &changeset, world);
		r = 0;
	} else {
		/* Failure -- print errors */
		apk_solver_print_errors(db, solution, world, r);
	}
	apk_package_array_free(&solution);
	apk_solver_free(db);

	return r;
}
