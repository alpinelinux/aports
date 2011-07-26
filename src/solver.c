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

/* FIXME: install-if not supported yet */

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
	unsigned short flags;
	unsigned short conflicts;
};

struct apk_name_state {
	struct list_head unsolved_list;
	struct apk_package *chosen;
	struct apk_package *preferred;
	unsigned short requirers;
};

struct apk_solver_state {
	struct apk_database *db;
	struct apk_package_state *pkg_state;
	struct apk_name_state *name_state;
	struct list_head unsolved_list_head;
	struct apk_package *latest_decision;
	unsigned int topology_position;
	unsigned int assigned_names;

	struct apk_package_array *best_solution;
	unsigned int best_cost;
};

static int apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static int undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static int push_decision(struct apk_solver_state *ss, struct apk_package *pkg,
			 int flags);

static inline int pkg_available(struct apk_database *db, struct apk_package *pkg)
{
	if (pkg->ipkg != NULL)
		return TRUE;
	if (pkg->installed_size == 0)
		return TRUE;
	if (pkg->filename != NULL)
		return TRUE;
	if (apk_db_select_repo(db, pkg) != NULL)
		return TRUE;
	return FALSE;
}

static int foreach_dependency(struct apk_solver_state *ss, struct apk_dependency_array *deps,
			      int (*func)(struct apk_solver_state *ss, struct apk_dependency *dep))
{
	int i, r = 0;
	for (i = 0; i < deps->num; i++)
		r += func(ss, &deps->item[i]);
	return r;
}

static void calculate_preferred(struct apk_database *db,
			        struct apk_name *name,
			        struct apk_name_state *ns)
{
	struct apk_package *installed = NULL, *latest = NULL;
	int i;

	/* Get latest and installed packages */
	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg = name->pkgs->item[i];

		if (pkg->ipkg != NULL)
			installed = pkg;
		else if (!pkg_available(db, pkg))
			continue;
		if (latest == NULL) {
			latest = pkg;
			continue;
		}

		if ((apk_flags & APK_PREFER_AVAILABLE) ||
		    (name->flags & APK_NAME_REINSTALL)) {
			if (latest->repos != 0 && pkg->repos == 0)
				continue;
			if (latest->repos == 0 && pkg->repos != 0) {
				latest = pkg;
				continue;
			}
			/* Otherwise both are not available, or both are
			 * available and we just compare the versions then */
		}
		if (apk_pkg_version_compare(pkg, latest) == APK_VERSION_GREATER)
			latest = pkg;
	}

	/* Choose the best looking candidate. */
	if (apk_flags & APK_UPGRADE) {
		ns->preferred = latest;
	} else {
		if (installed != NULL &&
		    (installed->repos != 0 ||
		     !(name->flags & APK_NAME_REINSTALL)))
			ns->preferred = installed;
		else
			ns->preferred = latest;
	}

}

static int update_name_state(struct apk_solver_state *ss,
			     struct apk_name *name, struct apk_name_state *ns)
{
	struct apk_package *pkg_best = NULL;
	int i, options = 0;

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = &ss->pkg_state[pkg0->topology_sort];

		if (pkg0->topology_sort >= ss->topology_position ||
		    ps0->conflicts != 0)
			continue;

		options++;
		if (pkg_best == NULL ||
		    pkg0->topology_sort > pkg_best->topology_sort)
			pkg_best = pkg0;
	}
	ns->chosen = pkg_best;
	dbg_printf("%s: adjusted preference %d -> %d (options left %d)\n",
		   name->name, ss->topology_position, ns->chosen->topology_sort,
		   options);
	return options;
}

static int apply_decision(struct apk_solver_state *ss,
			  struct apk_package *pkg,
			  struct apk_package_state *ps)
{
	struct apk_name_state *ns = &ss->name_state[pkg->name->id];

	dbg_printf("apply_decision: " PKG_VER_FMT " %s\n", PKG_VER_PRINTF(pkg),
		   (ps->flags & APK_PKGSTF_INSTALL) ? "INSTALL" : "NO_INSTALL");

	if (ps->flags & APK_PKGSTF_INSTALL) {
		ss->assigned_names++;
		ns->chosen = pkg;
		if (list_hashed(&ns->unsolved_list)) {
			list_del(&ns->unsolved_list);
			list_init(&ns->unsolved_list);
			dbg_printf("%s: deleting from unsolved list\n",
				   pkg->name->name);
		}
		return foreach_dependency(ss, pkg->depends, apply_constraint);
	} else {
		if (!list_hashed(&ns->unsolved_list)) {
			ns->chosen = NULL;
			return 0;
		}
		if (update_name_state(ss, pkg->name, ns) != 1)
			return 0;
		/* the name is required and we are left with only one candidate
		 * after deciding to not install pkg; autoselect the last option */
		return push_decision(ss, ns->chosen, APK_PKGSTF_INSTALL);
	}
}

static void undo_decision(struct apk_solver_state *ss,
			  struct apk_package *pkg,
			  struct apk_package_state *ps)
{
	struct apk_name_state *ns = &ss->name_state[pkg->name->id];

	dbg_printf("undo_decision: " PKG_VER_FMT " %s\n", PKG_VER_PRINTF(pkg),
		   (ps->flags & APK_PKGSTF_INSTALL) ? "INSTALL" : "NO_INSTALL");

	if (ps->flags & APK_PKGSTF_INSTALL) {
		ss->assigned_names--;
		foreach_dependency(ss, pkg->depends, undo_constraint);

		if (ns->requirers) {
			list_add(&ns->unsolved_list, &ss->unsolved_list_head);
			dbg_printf("%s: adding back to unsolved list (requirers: %d)\n",
				pkg->name->name, ns->requirers);
		} else {
			ns->chosen = NULL;
		}
	}
}

static int push_decision(struct apk_solver_state *ss, struct apk_package *pkg,
			 int flags)
{
	struct apk_package_state *ps = &ss->pkg_state[pkg->topology_sort];

	ps->backtrack = ss->latest_decision;
	ps->flags = flags;
	ss->latest_decision = pkg;
	if (flags & APK_PKGSTF_BRANCH) {
		ss->topology_position = pkg->topology_sort;
		dbg_printf("push_decision: adding new BRANCH at topology_position %d\n",
			   ss->topology_position);
	} else
		ps->flags |= APK_PKGSTF_ALT_BRANCH;

	return apply_decision(ss, pkg, ps);
}

static int next_branch(struct apk_solver_state *ss)
{
	struct apk_package *pkg;
	struct apk_package_state *ps;
	int r;

	while (1) {
		pkg = ss->latest_decision;
		ps = &ss->pkg_state[pkg->topology_sort];
		undo_decision(ss, pkg, ps);

		if (ps->flags & APK_PKGSTF_ALT_BRANCH) {
			pkg = ps->backtrack;
			ss->latest_decision = pkg;
			if (pkg == NULL) /* at root, can't back track */
				return 1;
			ss->topology_position = pkg->topology_sort;
			dbg_printf("next_branch: undo decision at topology_position %d\n",
				   ss->topology_position);
		} else {
			dbg_printf("next_branch: swapping BRANCH at topology_position %d\n",
				   ss->topology_position);

			ps->flags |= APK_PKGSTF_ALT_BRANCH;
			ps->flags ^= APK_PKGSTF_INSTALL;

			r = apply_decision(ss, pkg, ps);
			if (r == 0 /*|| report_errors */)
				return r;
		}
	}
}

static int apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_state *ns = &ss->name_state[name->id];
	struct apk_package *pkg_best = NULL;
	int i, options = 0;

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = &ss->pkg_state[pkg0->topology_sort];

		if (pkg0->topology_sort >= ss->topology_position) {
			dbg_printf(PKG_VER_FMT ": topology skip %d > %d\n",
				   PKG_VER_PRINTF(pkg0),
				   pkg0->topology_sort, ss->topology_position);
			continue;
		}

		if (!apk_dep_is_satisfied(dep, pkg0)) {
			ps0->conflicts++;
			dbg_printf(PKG_VER_FMT ": conflicts++ -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
		}
		if (ps0->conflicts == 0) {
			options++;
			if (pkg_best == NULL ||
			    pkg0->topology_sort > pkg_best->topology_sort)
				pkg_best = pkg0;
		}
	}

	ns->requirers++;
	if (!list_hashed(&ns->unsolved_list) && ns->chosen != NULL) {
		dbg_printf(PKG_VER_FMT " selected already for %s\n",  PKG_VER_PRINTF(ns->chosen),
			   dep->name->name);
		return !apk_dep_is_satisfied(dep, ns->chosen);
	}

	ns->chosen = pkg_best;
	if (options == 0) {
		/* we conflicted with all possible options */
		if (list_hashed(&ns->unsolved_list)) {
			dbg_printf("%s: deleting unsolved (unable to satisfy)\n",
				   name->name);
			list_del(&ns->unsolved_list);
			list_init(&ns->unsolved_list);
		}
		return 1;
	}
	if (options == 1) {
		/* we can short circuit to select the only option
		 * possible */
		return push_decision(ss, pkg_best, APK_PKGSTF_INSTALL);
	}
	/* multiple ways to satisfy the requirement */
	if (ns->requirers == 1) {
		list_init(&ns->unsolved_list);
		list_add(&ns->unsolved_list, &ss->unsolved_list_head);
		dbg_printf("%s: adding to unsolved list (%d options)\n",
			   name->name, options);
	}

	return 0;
}

static int undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_state *ns = &ss->name_state[name->id];
	struct apk_package *pkg_best = NULL;
	int i, had_options = 0, options = 0;

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = &ss->pkg_state[pkg0->topology_sort];

		if (pkg0->topology_sort >= ss->topology_position)
			continue;

		if (ps0->conflicts == 0)
			had_options++;
		if (!apk_dep_is_satisfied(dep, pkg0)) {
			ps0->conflicts--;
			dbg_printf(PKG_VER_FMT ": conflicts-- -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
		}
		if (ps0->conflicts == 0) {
			options++;
			if (pkg_best == NULL ||
			    pkg0->topology_sort > pkg_best->topology_sort)
				pkg_best = pkg0;
		}
	}

	ns->requirers--;

	if (ns->requirers == 0) {
		if (list_hashed(&ns->unsolved_list)) {
			list_del(&ns->unsolved_list);
			list_init(&ns->unsolved_list);
			ns->chosen = NULL;
		}
	} else {
		ns->chosen = pkg_best;
		if (had_options == 0 && options != 0) {
			if (!list_hashed(&ns->unsolved_list)) {
				list_add(&ns->unsolved_list, &ss->unsolved_list_head);
				dbg_printf("%s: adding back to unsolved list (with %d options, %d requirers)\n",
					   name->name, options, ns->requirers);
			} else {
				ns->chosen = NULL;
			}
			return 0;
		}
	}
	return 0;
}

static int expand_branch(struct apk_solver_state *ss)
{
	int r;

	while (1) {
		struct apk_name_state *ns;
		struct apk_package *pkg0 = NULL;

		/* FIXME: change unsolved_list to a priority queue */
		list_for_each_entry(ns, &ss->unsolved_list_head, unsolved_list) {
			if (pkg0 == NULL ||
			    ns->chosen->topology_sort > pkg0->topology_sort)
				pkg0 = ns->chosen;
		}
		if (pkg0 == NULL) {
			dbg_printf("expand_branch: list is empty\n");
			return 0;
		}

		/* someone needs to provide this name -- find next eligible
		 * provider candidate */
		ns = &ss->name_state[pkg0->name->id];
		dbg_printf("expand_branch: %s %d\n", pkg0->name->name, pkg0->topology_sort);

		/* Is there something we can still use? */
		if (ns->preferred == NULL)
			calculate_preferred(ss->db, pkg0->name, ns);

		r = push_decision(ss, pkg0,
			(pkg0 == ns->preferred) ?
			 (APK_PKGSTF_INSTALL | APK_PKGSTF_BRANCH) :
			 (APK_PKGSTF_NOINSTALL | APK_PKGSTF_BRANCH));

		if (/*no_error_reporting &&*/ r)
			return r;
	}

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
		ps = &ss->pkg_state[pkg->topology_sort];
		if (ps->flags & APK_PKGSTF_INSTALL)
			ss->best_solution->item[i++] = pkg;

		dbg_printf("record_solution: " PKG_VER_FMT ": %sINSTALL\n",
			   PKG_VER_PRINTF(pkg),
			   (ps->flags & APK_PKGSTF_INSTALL) ? "" : "NO_");

		pkg = ps->backtrack;
	}
}

int apk_solver_solve(struct apk_database *db, struct apk_dependency_array *world,
		     struct apk_package_array **solution)
{
	struct apk_solver_state *ss;
	int r;

	ss = calloc(1, sizeof(struct apk_solver_state));
	ss->db = db;
	ss->topology_position = -1;
	list_init(&ss->unsolved_list_head);
	ss->pkg_state = calloc(db->available.packages.num_items+1, sizeof(struct apk_package_state));
	ss->name_state = calloc(db->available.names.num_items+1, sizeof(struct apk_name_state));

	r = foreach_dependency(ss, world, apply_constraint);
	while (r == 0) {
		if (expand_branch(ss) == 0) {
			/* found solution*/
			/* FIXME: we should check other permutations if they
			 * have smaller cost to find optimal changeset */
			break;
		} else {
			/* conflicting constraints -- backtrack */
			r = next_branch(ss);
		}
	}

	/* collect packages */
	if (r == 0) {
		record_solution(ss);
		*solution = ss->best_solution;
	}

	free(ss->name_state);
	free(ss->pkg_state);
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
			return c1->oldpkg->topology_sort - c2->oldpkg->topology_sort;

		/* c1 deleted, c2 installed -> c2 first*/
		return -1;
	}
	if (c2->newpkg == NULL)
		/* c1 installed, c2 deleted -> c1 first*/
		return 1;

	return c2->newpkg->topology_sort - c1->newpkg->topology_sort;
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
