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
#define APK_PKGSTF_ALT_BRANCH		2

struct apk_score {
	unsigned short unsatisfiable;
	unsigned short preference;
};

struct apk_package_state {
	struct apk_package *backtrack;
	unsigned int topology_soft;
	unsigned short conflicts;
	unsigned availability_checked : 1;
	unsigned unavailable : 1;
	unsigned install_applied : 1;
	unsigned handle_install_if : 1;
	unsigned locked : 1;
	unsigned flags : 2;
};

struct apk_name_state {
	struct list_head unsolved_list;
	struct apk_name *name;
	struct apk_package *chosen;
	struct apk_score minimum_penalty;
	unsigned short requirers;
	unsigned short install_ifs;

	unsigned short preferred_pinning;
	unsigned short allowed_pinning;

	unsigned int solver_flags_local : 4;
	unsigned int solver_flags_local_mask : 4;
	unsigned int solver_flags_inherited : 4;

	unsigned int locked : 1;
	unsigned int in_changeset : 1;
};

struct apk_solver_state {
	struct apk_database *db;
	unsigned num_topology_positions;

	struct list_head unsolved_list_head;
	struct apk_package *latest_decision;
	unsigned int topology_position;
	unsigned int assigned_names;
	struct apk_score score;
	struct apk_score minimum_penalty;

	unsigned int solver_flags : 4;

	struct apk_solution_array *best_solution;
	struct apk_score best_score;
};

typedef enum {
	SOLVERR_SOLUTION = 0,
	SOLVERR_PRUNED,
	SOLVERR_EXPAND,
	SOLVERR_STOP,
} solver_result_t;

static void apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static void undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static solver_result_t push_decision(struct apk_solver_state *ss, struct apk_package *pkg,
				     int flags);

static void addscore(struct apk_score *a, struct apk_score *b)
{
	a->unsatisfiable += b->unsatisfiable;
	a->preference += b->preference;
}

static void subscore(struct apk_score *a, struct apk_score *b)
{
	a->unsatisfiable -= b->unsatisfiable;
	a->preference -= b->preference;
}

static int cmpscore(struct apk_score *a, struct apk_score *b)
{
	if (a->unsatisfiable < b->unsatisfiable)
		return -1;
	if (a->unsatisfiable > b->unsatisfiable)
		return 1;
	if (a->preference < b->preference)
		return -1;
	if (a->preference > b->preference)
		return 1;
	return 0;
}

static int cmpscore2(struct apk_score *a1, struct apk_score *a2, struct apk_score *b)
{
	if (a1->unsatisfiable+a2->unsatisfiable < b->unsatisfiable)
		return -1;
	if (a1->unsatisfiable+a2->unsatisfiable > b->unsatisfiable)
		return 1;
	if (a1->preference+a2->preference < b->preference)
		return -1;
	if (a1->preference+a2->preference > b->preference)
		return 1;
	return 0;
}

static struct apk_package_state *pkg_to_ps(struct apk_package *pkg)
{
	return (struct apk_package_state*) pkg->state_ptr;
}

static struct apk_name_state *name_to_ns(struct apk_name *name)
{
	struct apk_name_state *ns;

	if (name->state_ptr == NULL) {
		ns = calloc(1, sizeof(struct apk_name_state));
		ns->name = name;
		name->state_ptr = ns;
	} else {
		ns = (struct apk_name_state*) name->state_ptr;
	}
	return ns;
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
			if (!apk_dep_is_satisfied(dep, pkg0))
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
				    apk_dep_is_satisfied(dep, pkg))
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
	if (ps->topology_soft)
		return;
	pkg->topology_hard = -1;
	ps->topology_soft = -1;

	/* Consider hard dependencies only */
	foreach_dependency_pkg(ss, pkg->depends, sort_hard_dependencies);
	foreach_dependency_pkg(ss, pkg->install_if, sort_hard_dependencies);

	ps->topology_soft = pkg->topology_hard = ++ss->num_topology_positions;
	dbg_printf(PKG_VER_FMT ": topology_hard=%d\n",
		   PKG_VER_PRINTF(pkg), pkg->topology_hard);
}

static void sort_soft_dependencies(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_package_state *ps;

	sort_hard_dependencies(ss, pkg);

	ps = pkg_to_ps(pkg);
	if (ps->topology_soft != pkg->topology_hard)
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

static void foreach_locked_reverse_dependency(
	struct apk_name *name,
	void (*cb)(struct apk_package *rdepend, void *ctx), void *ctx)
{
	int i, j;

	if (name == NULL)
		return;

	for (i = 0; i < name->rdepends->num; i++) {
		struct apk_name *name0 = name->rdepends->item[i];
		struct apk_name_state *ns0 = name_to_ns(name0);
		struct apk_package *pkg0 = ns0->chosen;

		if (!ns0->locked || ns0->chosen == NULL)
			continue;

		for (j = 0; j < pkg0->depends->num; j++) {
			struct apk_dependency *dep = &pkg0->depends->item[j];
			if (dep->name == name)
				break;
		}
		if (j >= pkg0->depends->num)
			continue;

		cb(pkg0, ctx);
	}
}

static void foreach_dependency(struct apk_solver_state *ss, struct apk_dependency_array *deps,
			       void (*func)(struct apk_solver_state *ss, struct apk_dependency *dep))
{
	int i;
	for (i = 0; i < deps->num; i++)
		func(ss, &deps->item[i]);
}

static unsigned int get_pinning_mask_repos(struct apk_database *db, unsigned short pinning_mask)
{
	unsigned int repository_mask = 0;
	int i;

	for (i = 0; i < db->num_repo_tags && pinning_mask; i++) {
		if (!(BIT(i) & pinning_mask))
			continue;
		pinning_mask &= ~BIT(i);
		repository_mask |= db->repo_tags[i].allowed_repos;
	}
	return repository_mask;
}

static int compare_package_preference(unsigned short solver_flags,
				      unsigned int preferred_repos,
				      unsigned int allowed_repos,
				      struct apk_package *pkgA,
				      struct apk_package *pkgB,
				      struct apk_database *db)
{
	unsigned int a_repos, b_repos;

	/* specified on command line directly */
	if (pkgA->filename && !pkgB->filename)
		return 1;
	if (pkgB->filename && !pkgA->filename)
		return -1;

	/* Is there a difference in pinning preference? */
	a_repos = pkgA->repos | (pkgA->ipkg ? db->repo_tags[pkgA->ipkg->repository_tag].allowed_repos : 0);
	b_repos = pkgB->repos | (pkgB->ipkg ? db->repo_tags[pkgB->ipkg->repository_tag].allowed_repos : 0);
	if ((a_repos & preferred_repos) && !(b_repos & preferred_repos))
		return 1;
	if ((b_repos & preferred_repos) && !(a_repos & preferred_repos))
		return -1;

	/* Difference in allowed repositories? */
	if ((a_repos & allowed_repos) && !(b_repos & allowed_repos))
		return 1;
	if ((b_repos & allowed_repos) && !(a_repos & allowed_repos))
		return -1;

	if (solver_flags & APK_SOLVERF_AVAILABLE) {
		if (pkgA->repos != 0 && pkgB->repos == 0)
			return 1;
		if (pkgB->repos != 0 && pkgA->repos == 0)
			return -1;
	}

	if (!(solver_flags & APK_SOLVERF_UPGRADE)) {
		/* not upgrading, prefer the installed package */
		if (pkgA->ipkg != NULL && pkgB->ipkg == NULL)
			return 1;
		if (pkgB->ipkg != NULL && pkgA->ipkg == NULL)
			return -1;
	}

	/* upgrading, or neither of the package is installed, so
	 * we just fall back comparing to versions */
	switch (apk_pkg_version_compare(pkgA, pkgB)) {
	case APK_VERSION_GREATER:
		return 1;
	case APK_VERSION_LESS:
		return -1;
	}

	/* upgrading, prefer the installed package */
	if (pkgA->ipkg != NULL && pkgB->ipkg == NULL)
		return 1;
	if (pkgB->ipkg != NULL && pkgA->ipkg == NULL)
		return -1;

	/* prefer the one with lowest available repository */
	return ffsl(pkgB->repos) - ffsl(pkgA->repos);
}

static int get_preference(struct apk_solver_state *ss,
			  struct apk_package *pkg,
			  int installable_only)
{
	struct apk_name *name = pkg->name;
	struct apk_name_state *ns = name_to_ns(name);
	unsigned short name_flags = ns->solver_flags_local
		| ns->solver_flags_inherited
		| ss->solver_flags;
	unsigned short preferred_pinning, allowed_pinning;
	unsigned int preferred_repos, allowed_repos;
	unsigned short preference = 0;
	int i, r;

	preferred_pinning = ns->preferred_pinning ?: APK_DEFAULT_PINNING_MASK;
	preferred_repos = get_pinning_mask_repos(ss->db, preferred_pinning);
	allowed_pinning = ns->allowed_pinning | preferred_pinning;
	if (preferred_pinning != allowed_pinning)
		allowed_repos = get_pinning_mask_repos(ss->db, allowed_pinning);
	else
		allowed_repos = preferred_repos;

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (pkg0 == pkg || ps0 == NULL)
			continue;

		if (installable_only &&
		    (ss->topology_position < pkg0->topology_hard ||
		     ps0->locked))
			continue;

		r = compare_package_preference(name_flags,
					       preferred_repos,
					       allowed_repos,
					       pkg, pkg0, ss->db);
		if (r < 0)
			preference += -r;
	}

	return preference;
}

static int install_if_missing(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_name_state *ns;
	int i, missing = 0;

	for (i = 0; i < pkg->install_if->num; i++) {
		struct apk_dependency *dep = &pkg->install_if->item[i];

		ns = name_to_ns(dep->name);
		if (!ns->locked || !apk_dep_is_satisfied(dep, ns->chosen))
			missing++;
	}

	return missing;
}

static int check_if_package_unavailable(struct apk_solver_state *ss, struct apk_package *pkg, int do_check)
{
	struct apk_name *name = pkg->name;
	struct apk_package_state *ps = pkg_to_ps(pkg);
	struct apk_name_state *ns = name_to_ns(name);

	/* installed and no-reinstall required? no check needed. */
	if ((pkg->ipkg != NULL) &&
	    ((ns->solver_flags_local | ns->solver_flags_inherited |
	      ss->solver_flags) & APK_SOLVERF_REINSTALL) == 0)
		return 0;

	/* done already? */
	if (ps->availability_checked && !do_check)
		return ps->unavailable;

	/* and it's not available, we can't use it */
	if (!pkg_available(ss->db, pkg))
		ps->unavailable = 1;

	ps->availability_checked = 1;
	return ps->unavailable;
}

static int update_name_state(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name_state *ns = name_to_ns(name);
	struct apk_package *best_pkg = NULL, *preferred_pkg = NULL;
	struct apk_package_state *preferred_ps = NULL;
	unsigned int best_topology = 0;
	unsigned short name_flags = ns->solver_flags_local
		| ns->solver_flags_inherited
		| ss->solver_flags;
	unsigned short preferred_pinning, allowed_pinning;
	unsigned int preferred_repos, allowed_repos;
	int i, options = 0;

	if (ns->locked)
		return ns->chosen != NULL;

	preferred_pinning = ns->preferred_pinning ?: APK_DEFAULT_PINNING_MASK;
	preferred_repos = get_pinning_mask_repos(ss->db, preferred_pinning);
	allowed_pinning = ns->allowed_pinning | preferred_pinning;
	if (preferred_pinning != allowed_pinning)
		allowed_repos = get_pinning_mask_repos(ss->db, allowed_pinning);
	else
		allowed_repos = preferred_repos;

	subscore(&ss->minimum_penalty, &ns->minimum_penalty);
	ns->minimum_penalty = (struct apk_score) { 0, 0 };

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0 == NULL ||
		    ss->topology_position < pkg0->topology_hard ||
		    ps0->locked ||
		    check_if_package_unavailable(ss, pkg0, 0))
			continue;

		options++;

		/* preferred - currently most optimal for end solution */
		if ((preferred_pkg == NULL) ||
		    (ps0->conflicts < preferred_ps->conflicts) ||
		    (ps0->conflicts == preferred_ps->conflicts &&
		     compare_package_preference(name_flags,
						preferred_repos,
						allowed_repos,
						pkg0, preferred_pkg, ss->db) > 0)) {
			preferred_pkg = pkg0;
			preferred_ps = ps0;
		}

		/* next in topology order - next to get locked in */
		if (ps0->topology_soft < ss->topology_position &&
		    ps0->topology_soft > best_topology)
			best_pkg = pkg0, best_topology = ps0->topology_soft;
		else if (pkg0->topology_hard > best_topology)
			best_pkg = pkg0, best_topology = pkg0->topology_hard;
	}

	if (ns->requirers == 0 && ns->install_ifs == 0) {
		if (list_hashed(&ns->unsolved_list)) {
			list_del(&ns->unsolved_list);
			list_init(&ns->unsolved_list);
			ns->chosen = NULL;
		}
		dbg_printf("%s: deleted from unsolved: %d requirers, %d install_ifs, %d options\n",
			   name->name, ns->requirers, ns->install_ifs, options);
	} else {
		if (!list_hashed(&ns->unsolved_list))
			list_add(&ns->unsolved_list, &ss->unsolved_list_head);

		ns->chosen = best_pkg;
		if (preferred_pkg != NULL &&
		    preferred_ps->conflicts < ns->requirers) {
			ns->minimum_penalty = (struct apk_score) {
				.unsatisfiable = preferred_ps->conflicts,
				.preference = get_preference(ss, preferred_pkg, FALSE),
			};
			dbg_printf("%s: min.penalty for name {%d, %d} from pkg " PKG_VER_FMT "\n",
				   name->name,
				   ns->minimum_penalty.unsatisfiable,
				   ns->minimum_penalty.preference,
				   PKG_VER_PRINTF(preferred_pkg));
		} else {
			ns->minimum_penalty = (struct apk_score) {
				.unsatisfiable = ns->requirers,
				.preference = name->pkgs->num,
			};
			dbg_printf("%s: min.penalty for name {%d, %d} from no pkg\n",
				   name->name,
				   ns->minimum_penalty.unsatisfiable,
				   ns->minimum_penalty.preference);
		}
		addscore(&ss->minimum_penalty, &ns->minimum_penalty);

		dbg_printf("%s: added to unsolved: %d requirers, %d install_ifs, %d options (next topology %d)\n",
			   name->name, ns->requirers, ns->install_ifs,
			   options, best_topology);
	}

	return options;
}

static void trigger_install_if(struct apk_solver_state *ss,
			       struct apk_package *pkg)
{
	if (install_if_missing(ss, pkg) == 0) {
		struct apk_name_state *ns = ns = name_to_ns(pkg->name);

		dbg_printf("trigger_install_if: " PKG_VER_FMT " triggered\n",
			   PKG_VER_PRINTF(pkg));
		ns->install_ifs++;
		update_name_state(ss, pkg->name);
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
		update_name_state(ss, pkg->name);
	}
}

static solver_result_t apply_decision(struct apk_solver_state *ss,
				      struct apk_package *pkg,
				      struct apk_package_state *ps)
{
	struct apk_name *name = pkg->name;
	struct apk_name_state *ns = name_to_ns(name);
	unsigned short preference;

	dbg_printf("-->apply_decision: " PKG_VER_FMT " %s\n", PKG_VER_PRINTF(pkg),
		   (ps->flags & APK_PKGSTF_INSTALL) ? "INSTALL" : "NO_INSTALL");

	if (ps->flags & APK_PKGSTF_INSTALL) {
		subscore(&ss->minimum_penalty, &ns->minimum_penalty);
		ns->minimum_penalty = (struct apk_score) { 0, 0 };

		preference = get_preference(ss, pkg, FALSE);
		ss->score.unsatisfiable += ps->conflicts;
		ss->score.preference += preference;
		if (cmpscore2(&ss->score, &ss->minimum_penalty, &ss->best_score) >= 0 ||
		    check_if_package_unavailable(ss, pkg, 1)) {
			dbg_printf("install causing {%d,%d}, penalty too big (or unavailable %d): {%d,%d}+{%d,%d}>={%d,%d}\n",
				ps->conflicts, preference,
				ss->score.unsatisfiable, ss->score.preference,
				ss->minimum_penalty.unsatisfiable, ss->minimum_penalty.preference,
				ss->best_score.unsatisfiable, ss->best_score.preference
				);

			ss->score.unsatisfiable -= ps->conflicts;
			ss->score.preference -= preference;
			return SOLVERR_PRUNED;
		}

		ps->install_applied = 1;
		ss->assigned_names++;
		ns->chosen = pkg;
		ns->locked = 1;

		list_del(&ns->unsolved_list);
		list_init(&ns->unsolved_list);
		dbg_printf("%s: deleting from unsolved list\n",
			   pkg->name->name);

		foreach_dependency(ss, pkg->depends, apply_constraint);
		foreach_rinstall_if_pkg(ss, pkg, trigger_install_if);
	} else {
		if (update_name_state(ss, pkg->name) == 0) {
			subscore(&ss->minimum_penalty, &ns->minimum_penalty);
			ns->minimum_penalty = (struct apk_score) { 0, 0 };

			dbg_printf("%s: out of candidates, locking to none\n",
				   pkg->name->name);

			ss->score.unsatisfiable += ns->requirers;
			ss->score.preference += name->pkgs->num;
			ns->locked = 1;
		}
	}

	if (cmpscore2(&ss->score, &ss->minimum_penalty, &ss->best_score) >= 0) {
		dbg_printf("install/uninstall finished penalty too big: {%d,%d}+{%d,%d}>={%d,%d}\n",
			ss->score.unsatisfiable, ss->score.preference,
			ss->minimum_penalty.unsatisfiable, ss->minimum_penalty.preference,
			ss->best_score.unsatisfiable, ss->best_score.preference
			);
		return SOLVERR_PRUNED;
	}

	return SOLVERR_EXPAND;
}

static void undo_decision(struct apk_solver_state *ss,
			  struct apk_package *pkg,
			  struct apk_package_state *ps)
{
	struct apk_name *name = pkg->name;
	struct apk_name_state *ns = name_to_ns(name);

	dbg_printf("-->undo_decision: " PKG_VER_FMT " %s\n", PKG_VER_PRINTF(pkg),
		   (ps->flags & APK_PKGSTF_INSTALL) ? "INSTALL" : "NO_INSTALL");

	if (ps->handle_install_if)
		ss->topology_position = ps->topology_soft;
	else
		ss->topology_position = pkg->topology_hard;

	if (ps->flags & APK_PKGSTF_INSTALL) {
		if (ps->install_applied) {
			unsigned short preference;

			ps->install_applied = 0;
			ss->assigned_names--;
			foreach_rinstall_if_pkg(ss, pkg, untrigger_install_if);
			foreach_dependency(ss, pkg->depends, undo_constraint);

			preference = get_preference(ss, pkg, FALSE);
			ss->score.unsatisfiable -= ps->conflicts;
			ss->score.preference -= preference;
		}
	} else {
		if (ns->locked) {
			/* UNINSTALL decision removed - either name is unlocked
			 * or locked to none */
			ss->score.unsatisfiable -= ns->requirers;
			ss->score.preference -= name->pkgs->num;
		}
	}
	ns->locked = 0;
	ns->chosen = NULL;

	update_name_state(ss, pkg->name);
}

static solver_result_t push_decision(struct apk_solver_state *ss, struct apk_package *pkg,
				     int flags)
{
	struct apk_package_state *ps = pkg_to_ps(pkg);

	ps->backtrack = ss->latest_decision;
	ps->flags = flags;
	ps->locked = 1;
	ps->handle_install_if = 0;

	if (ps->topology_soft < ss->topology_position) {
		if (flags & APK_PKGSTF_INSTALL)
			ps->handle_install_if = 1;
		ss->topology_position = ps->topology_soft;
	} else {
		ss->topology_position = pkg->topology_hard;
	}
	ss->latest_decision = pkg;

	dbg_printf("-->push_decision: adding new BRANCH at topology_position %d (score: unsatisfied %d, preference %d)\n",
		   ss->topology_position,
		   ss->score.unsatisfiable,
		   ss->score.preference);

	if (ps->handle_install_if)
		dbg_printf("triggers due to " PKG_VER_FMT "\n",
			   PKG_VER_PRINTF(pkg));

	return apply_decision(ss, pkg, ps);
}

static int next_branch(struct apk_solver_state *ss)
{
	struct apk_package *pkg;
	struct apk_package_state *ps;

	while (ss->latest_decision != NULL) {
		pkg = ss->latest_decision;
		ps = pkg_to_ps(pkg);

		if (ps->flags & APK_PKGSTF_ALT_BRANCH) {
			dbg_printf("-->next_branch: undo decision at topology_position %d\n",
				   ss->topology_position);
			ps->flags &= ~APK_PKGSTF_ALT_BRANCH;
			ps->locked = 0;
			undo_decision(ss, pkg, ps);

			ss->latest_decision = ps->backtrack;
		} else {
			dbg_printf("-->next_branch: swapping BRANCH at topology_position %d\n",
				   ss->topology_position);

			undo_decision(ss, pkg, ps);

			ps->flags |= APK_PKGSTF_ALT_BRANCH;
			ps->flags ^= APK_PKGSTF_INSTALL;

			return apply_decision(ss, pkg, ps);
		}
	}

	dbg_printf("-->next_branch: no more branches\n");
	return SOLVERR_STOP;
}

static void inherit_name_state(struct apk_name *to, struct apk_name *from)
{
	struct apk_name_state *tns = name_to_ns(to);
	struct apk_name_state *fns = name_to_ns(from);

	tns->solver_flags_inherited |=
		fns->solver_flags_inherited |
		(fns->solver_flags_local & fns->solver_flags_local_mask);

	tns->allowed_pinning |= fns->allowed_pinning | fns->preferred_pinning;
}

static void inherit_name_state_wrapper(struct apk_package *rdepend, void *ctx)
{
	struct apk_name *name = (struct apk_name *) ctx;
	inherit_name_state(name, rdepend->name);
}

static int has_inherited_state(struct apk_name *name)
{
	struct apk_name_state *ns = name_to_ns(name);

	if (name == NULL)
		return 0;
	if (ns->solver_flags_inherited || (ns->solver_flags_local & ns->solver_flags_local_mask))
		return 1;
	if (ns->allowed_pinning)
		return 1;
	return 0;
}

static void recalculate_inherted_name_state(struct apk_name *name)
{
	struct apk_name_state *ns = name_to_ns(name);

	ns->solver_flags_inherited = 0;
	ns->allowed_pinning = 0;
	foreach_locked_reverse_dependency(name, inherit_name_state_wrapper, name);
}

static void apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_state *ns = name_to_ns(name);
	int i;

	if (ns->locked) {
		if (ns->chosen)
			dbg_printf("%s: locked to " PKG_VER_FMT " already\n",
				   name->name, PKG_VER_PRINTF(ns->chosen));
		else
			dbg_printf("%s: locked to empty\n",
				   name->name);
		if (!apk_dep_is_satisfied(dep, ns->chosen))
			ss->score.unsatisfiable++;
		return;
	}
	if (name->pkgs->num == 0) {
		if (!dep->optional)
			ss->score.unsatisfiable++;
		return;
	}

	if (dep->repository_tag) {
		dbg_printf("%s: adding pinnings %d\n",
			   dep->name->name, dep->repository_tag);
		ns->preferred_pinning = BIT(dep->repository_tag);
		ns->allowed_pinning |= BIT(dep->repository_tag);
	}

	if (ss->latest_decision != NULL) {
		dbg_printf("%s: inheriting flags and pinning from %s\n",
			   name->name, ss->latest_decision->name->name);
		inherit_name_state(name, ss->latest_decision->name);
	}

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0 == NULL ||
		    pkg0->topology_hard >= ss->topology_position)
			continue;

		if (!apk_dep_is_satisfied(dep, pkg0)) {
			ps0->conflicts++;
			dbg_printf(PKG_VER_FMT ": conflicts++ -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
		}
	}

	if (!dep->optional)
		ns->requirers++;

	update_name_state(ss, name);
}

static void undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_state *ns = name_to_ns(name);
	int i;

	if (ns->locked) {
		if (ns->chosen != NULL) {
			dbg_printf(PKG_VER_FMT " selected already for %s\n",
				   PKG_VER_PRINTF(ns->chosen), name->name);
		} else {
			dbg_printf("%s selected to not be satisfied\n",
				   name->name);
		}
		if (!apk_dep_is_satisfied(dep, ns->chosen))
			ss->score.unsatisfiable--;
		return;
	}
	if (name->pkgs->num == 0) {
		if (!dep->optional)
			ss->score.unsatisfiable--;
		return;
	}

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg0 = name->pkgs->item[i];
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (pkg0->topology_hard >= ss->topology_position)
			continue;

		if (!apk_dep_is_satisfied(dep, pkg0)) {
			ps0->conflicts--;
			dbg_printf(PKG_VER_FMT ": conflicts-- -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
		}
	}

	if (ss->latest_decision && has_inherited_state(ss->latest_decision->name))
		recalculate_inherted_name_state(name);

	if (!dep->optional)
		ns->requirers--;

	update_name_state(ss, name);
}

static int expand_branch(struct apk_solver_state *ss)
{
	struct apk_name_state *ns;
	struct apk_package *pkg0 = NULL;
	unsigned int topology0 = 0;
	unsigned short allowed_pinning, preferred_pinning;
	unsigned int allowed_repos;
	int flags;

	/* FIXME: change unsolved_list to a priority queue */
	list_for_each_entry(ns, &ss->unsolved_list_head, unsolved_list) {
		if (ns->chosen == NULL)
			continue;
		if (pkg_to_ps(ns->chosen)->topology_soft < ss->topology_position &&
		    pkg_to_ps(ns->chosen)->topology_soft > topology0)
			pkg0 = ns->chosen, topology0 = pkg_to_ps(pkg0)->topology_soft;
		else if (ns->chosen->topology_hard > topology0)
			pkg0 = ns->chosen, topology0 = pkg0->topology_hard;
	}
	if (pkg0 == NULL) {
		dbg_printf("expand_branch: list is empty (%d unsatisfied)\n",
			   ss->score.unsatisfiable);
		return SOLVERR_SOLUTION;
	}

	/* someone needs to provide this name -- find next eligible
	 * provider candidate */
	ns = name_to_ns(pkg0->name);
	dbg_printf("expand_branch: %s\n", pkg0->name->name);

	preferred_pinning = ns->preferred_pinning ?: APK_DEFAULT_PINNING_MASK;
	allowed_pinning = ns->allowed_pinning | preferred_pinning;
	allowed_repos = get_pinning_mask_repos(ss->db, allowed_pinning);

	if ((pkg0->repos != 0) && (pkg0->ipkg == NULL) &&
	    (pkg0->filename == NULL) && !(pkg0->repos & allowed_repos)) {
		/* pinning has not enabled the package */
		flags = APK_PKGSTF_NOINSTALL | APK_PKGSTF_ALT_BRANCH;
	} else if (ns->requirers == 0 && ns->install_ifs != 0 &&
		   install_if_missing(ss, pkg0)) {
		/* not directly required, and package specific
		 * install_if never triggered */
		flags = APK_PKGSTF_NOINSTALL | APK_PKGSTF_ALT_BRANCH;
	} else if (get_preference(ss, pkg0, TRUE) == 0) {
		flags = APK_PKGSTF_INSTALL;
	} else {
		flags = APK_PKGSTF_NOINSTALL;
	}

	return push_decision(ss, pkg0, flags);
}

static int get_tag(struct apk_database *db, unsigned short pinning_mask, unsigned int repos)
{
	int i;

	for (i = 0; i < db->num_repo_tags; i++) {
		if (!(BIT(i) & pinning_mask))
			continue;
		if (db->repo_tags[i].allowed_repos & repos)
			return i;
	}
	return APK_DEFAULT_REPOSITORY_TAG;
}

static void record_solution(struct apk_solver_state *ss)
{
	struct apk_database *db = ss->db;
	struct apk_package *pkg;
	struct apk_package_state *ps;
	struct apk_name_state *ns;
	int i;

	apk_solution_array_resize(&ss->best_solution, ss->assigned_names);

	i = 0;
	pkg = ss->latest_decision;
	while (pkg != NULL) {
		ps = pkg_to_ps(pkg);
		ns = name_to_ns(pkg->name);
		if (ps->flags & APK_PKGSTF_INSTALL) {
			unsigned short pinning;
			unsigned int repos;

			if (i >= ss->assigned_names)
				abort();

			pinning = ns->allowed_pinning | ns->preferred_pinning | APK_DEFAULT_PINNING_MASK;
			repos = pkg->repos | (pkg->ipkg ? db->repo_tags[pkg->ipkg->repository_tag].allowed_repos : 0);

			ss->best_solution->item[i++] = (struct apk_solution_entry){
				.pkg = pkg,
				.reinstall = !!((ns->solver_flags_local | ns->solver_flags_inherited |
					         ss->solver_flags) & APK_SOLVERF_REINSTALL),
				.repository_tag = get_tag(db, pinning, repos),
			};
		}

		dbg_printf("record_solution: " PKG_VER_FMT ": %sINSTALL\n",
			   PKG_VER_PRINTF(pkg),
			   (ps->flags & APK_PKGSTF_INSTALL) ? "" : "NO_");

		pkg = ps->backtrack;
	}
	apk_solution_array_resize(&ss->best_solution, i);
}

static int compare_solution_entry(const void *p1, const void *p2)
{
	const struct apk_solution_entry *c1 = (const struct apk_solution_entry *) p1;
	const struct apk_solution_entry *c2 = (const struct apk_solution_entry *) p2;

	return strcmp(c1->pkg->name->name, c2->pkg->name->name);
}

static int compare_change(const void *p1, const void *p2)
{
	const struct apk_change *c1 = (const struct apk_change *) p1;
	const struct apk_change *c2 = (const struct apk_change *) p2;

	if (c1->newpkg == NULL) {
		if (c2->newpkg == NULL) {
			/* both deleted - reverse topology order */
			return	c2->oldpkg->topology_hard -
				c1->oldpkg->topology_hard;
		}

		/* c1 deleted, c2 installed -> c2 first*/
		return 1;
	}
	if (c2->newpkg == NULL)
		/* c1 installed, c2 deleted -> c1 first*/
		return -1;

	return	c1->newpkg->topology_hard -
		c2->newpkg->topology_hard;
}

static int generate_changeset(struct apk_database *db,
			      struct apk_solution_array *solution,
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
		pkg = solution->item[i].pkg;
		ns = name_to_ns(pkg->name);
		ns->chosen = pkg;
		ns->in_changeset = 1;
		if ((pkg->ipkg == NULL) ||
		    solution->item[i].reinstall ||
		    solution->item[i].repository_tag != pkg->ipkg->repository_tag)
			num_installs++;
	}
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		name = ipkg->pkg->name;
		ns = name_to_ns(name);
		if ((ns->chosen == NULL) || !ns->in_changeset)
			num_removed++;
	}

	/* construct changeset */
	apk_change_array_resize(&changeset->changes, num_installs + num_removed);
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		name = ipkg->pkg->name;
		ns = name_to_ns(name);
		if ((ns->chosen == NULL) || !ns->in_changeset) {
			changeset->changes->item[ci].oldpkg = ipkg->pkg;
			ci++;
		}
	}
	for (i = 0; i < solution->num; i++) {
		pkg = solution->item[i].pkg;
		name = pkg->name;
		ns = name_to_ns(name);

		if ((pkg->ipkg == NULL) ||
		    solution->item[i].reinstall ||
		    solution->item[i].repository_tag != pkg->ipkg->repository_tag){
			for (j = 0; j < name->pkgs->num; j++) {
				pkg0 = name->pkgs->item[j];
				if (pkg0->ipkg == NULL)
					continue;
				changeset->changes->item[ci].oldpkg = pkg0;
				break;
			}
			changeset->changes->item[ci].newpkg = pkg;
			changeset->changes->item[ci].repository_tag = solution->item[i].repository_tag;
			changeset->changes->item[ci].reinstall = solution->item[i].reinstall;
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
			       unsigned short solver_flags,
			       unsigned short solver_flags_inheritable)
{
	struct apk_name_state *ns = name_to_ns(name);
	ns->solver_flags_local = solver_flags;
	ns->solver_flags_local_mask = solver_flags_inheritable;
}

static void apk_solver_free(struct apk_database *db)
{
	apk_hash_foreach(&db->available.names, free_state, NULL);
	apk_hash_foreach(&db->available.packages, free_package, NULL);
}

int apk_solver_solve(struct apk_database *db,
		     unsigned short solver_flags,
		     struct apk_dependency_array *world,
		     struct apk_solution_array **solution,
		     struct apk_changeset *changeset)
{
	struct apk_solver_state *ss;
	struct apk_installed_package *ipkg;
	struct apk_score zero_score;
	solver_result_t r = SOLVERR_STOP;
	int i;

	ss = calloc(1, sizeof(struct apk_solver_state));
	ss->db = db;
	ss->solver_flags = solver_flags;
	ss->topology_position = -1;
	ss->best_score = (struct apk_score){ .unsatisfiable = -1, .preference = -1 };
	list_init(&ss->unsolved_list_head);

	for (i = 0; i < world->num; i++)
		sort_name(ss, world->item[i].name);
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list)
		sort_name(ss, ipkg->pkg->name);

	foreach_dependency(ss, world, apply_constraint);
	zero_score = ss->score;

	dbg_printf("solver_solve: zero score: %d unsatisfiable, %d preference\n",
		   zero_score.unsatisfiable,
		   zero_score.preference);

	do {
		/* need EXPAND if here, can return SOLUTION|PRUNED|EXPAND */
		r = expand_branch(ss);
		if (r == SOLVERR_SOLUTION) {
			struct apk_score score;

			dbg_printf("solution with: %d unsatisfiable, %d preference\n",
				   ss->score.unsatisfiable,
				   ss->score.preference);

			score = ss->score;
			addscore(&score, &ss->minimum_penalty);

			if (cmpscore(&score, &ss->best_score) < 0) {
				record_solution(ss);
				ss->best_score = score;
			}

			if (cmpscore(&zero_score, &score) >= 0) {
				/* found solution - it is optimal because we permutate
				 * each preferred local option first, and permutations
				 * happen in topologally sorted order. */
				break;
			}
			r = SOLVERR_PRUNED;
		}
		/* next_branch() returns PRUNED, STOP or EXPAND */
		while (r == SOLVERR_PRUNED)
			r = next_branch(ss);
		/* STOP or EXPAND */
	} while (r != SOLVERR_STOP);

	/* collect packages */
	if (changeset != NULL) {
		generate_changeset(db, ss->best_solution, changeset,
				   ss->solver_flags);
	}
	if (solution != NULL) {
		qsort(ss->best_solution->item, ss->best_solution->num,
		      sizeof(struct apk_solution_entry), compare_solution_entry);
		*solution = ss->best_solution;
	} else {
		apk_solution_array_free(&ss->best_solution);
	}
	i = ss->best_score.unsatisfiable;
	apk_solver_free(db);
	free(ss);

	return i;
}

static void print_change(struct apk_database *db,
			 struct apk_change *change,
			 int cur, int total)
{
	struct apk_name *name;
	struct apk_package *oldpkg = change->oldpkg;
	struct apk_package *newpkg = change->newpkg;
	const char *msg = NULL;
	char status[32], n[512], *nameptr;
	int r;

	snprintf(status, sizeof(status), "(%i/%i)", cur+1, total);
	status[sizeof(status) - 1] = 0;

	name = newpkg ? newpkg->name : oldpkg->name;
	if (change->repository_tag > 0) {
		snprintf(n, sizeof(n), "%s@" BLOB_FMT,
			 name->name,
			 BLOB_PRINTF(*db->repo_tags[change->repository_tag].name));
		n[sizeof(n) - 1] = 0;
		nameptr = n;
	} else {
		nameptr = name->name;
	}

	if (oldpkg == NULL) {
		apk_message("%s Installing %s (" BLOB_FMT ")",
			    status, nameptr,
			    BLOB_PRINTF(*newpkg->version));
	} else if (newpkg == NULL) {
		apk_message("%s Purging %s (" BLOB_FMT ")",
			    status, nameptr,
			    BLOB_PRINTF(*oldpkg->version));
	} else if (newpkg == oldpkg && !change->reinstall) {
		apk_message("%s Updating pinning %s (" BLOB_FMT ")",
			    status, nameptr,
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
			    status, msg, nameptr,
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
			printf("%s:\n", msg);
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

static int compare_package_topology(const void *p1, const void *p2)
{
	struct apk_package *pkg1 = *(struct apk_package **) p1;
	struct apk_package *pkg2 = *(struct apk_package **) p2;

	return pkg1->topology_hard - pkg2->topology_hard;
}

static void run_triggers(struct apk_database *db)
{
	struct apk_package_array *pkgs;
	int i;

	pkgs = apk_db_get_pending_triggers(db);
	if (pkgs == NULL || pkgs->num == 0)
		return;

	qsort(pkgs->item, pkgs->num, sizeof(struct apk_package *),
	      compare_package_topology);

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

	if (apk_db_check_world(db, world) != 0) {
		apk_error("Not committing changes due to missing repository tags. Use --force to override.");
		return -1;
	}

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
			if (change->oldpkg != change->newpkg || change->reinstall)
				r = apk_db_install_pkg(db,
						       change->oldpkg, change->newpkg,
						       (apk_flags & APK_PROGRESS) ? progress_cb : NULL,
						       &prog);
			if (r != 0)
				break;
			if (change->newpkg)
				change->newpkg->ipkg->repository_tag = change->repository_tag;
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
		if (apk_verbosity > 1) {
			apk_message("OK: %d packages, %d dirs, %d files, %zu MiB",
				    db->installed.stats.packages,
				    db->installed.stats.dirs,
				    db->installed.stats.files,
				    db->installed.stats.bytes / (1024 * 1024));
		} else {
			apk_message("OK: %zu MiB in %d packages",
				    db->installed.stats.bytes / (1024 * 1024),
				    db->installed.stats.packages);
		}
	}

	return r;
}

static void print_dep_errors(struct apk_database *db, char *label, struct apk_dependency_array *deps)
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
		apk_blob_push_dep(&p, db, dep);
		p = apk_blob_pushed(APK_BLOB_BUF(buf), p);
		apk_print_indented(&indent, p);
	}
	if (!print_label)
		printf("\n");
}

void apk_solver_print_errors(struct apk_database *db,
			     struct apk_solution_array *solution,
			     struct apk_dependency_array *world,
			     int unsatisfiable)
{
	int i;

	apk_error("%d unsatisfiable dependencies:", unsatisfiable);

	for (i = 0; i < solution->num; i++) {
		struct apk_package *pkg = solution->item[i].pkg;
		pkg->name->state_ptr = pkg;
	}

	print_dep_errors(db, "world", world);
	for (i = 0; i < solution->num; i++) {
		struct apk_package *pkg = solution->item[i].pkg;
		char pkgtext[256];
		snprintf(pkgtext, sizeof(pkgtext), PKG_VER_FMT, PKG_VER_PRINTF(pkg));
		print_dep_errors(db, pkgtext, pkg->depends);
	}
}

int apk_solver_commit(struct apk_database *db,
		      unsigned short solver_flags,
		      struct apk_dependency_array *world)
{
	struct apk_changeset changeset = {};
	struct apk_solution_array *solution = NULL;
	int r;

	if (apk_db_check_world(db, world) != 0) {
		apk_error("Not committing changes due to missing repository tags. Use --force to override.");
		return -1;
	}

	r = apk_solver_solve(db, solver_flags,
			     world, &solution, &changeset);
	if (r < 0)
		return r;

	if (r == 0 || (apk_flags & APK_FORCE)) {
		/* Success -- or forced installation of bad graph */
		r = apk_solver_commit_changeset(db, &changeset, world);
	} else {
		/* Failure -- print errors */
		apk_solver_print_errors(db, solution, world, r);
	}
	apk_solution_array_free(&solution);
	apk_change_array_free(&changeset.changes);

	return r;
}
