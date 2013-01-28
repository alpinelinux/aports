/* solver.c - Alpine Package Keeper (APK)
 * A backtracking, forward checking dependency graph solver.
 *
 * Copyright (C) 2008-2012 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdint.h>
#include <unistd.h>
#include "apk_defines.h"
#include "apk_database.h"
#include "apk_package.h"
#include "apk_solver.h"

#include "apk_print.h"

//#define DEBUG_PRINT
#define DEBUG_CHECKS

#ifdef DEBUG_PRINT
#include <stdio.h>
#define dbg_printf(args...) fprintf(stderr, args)
#else
#define dbg_printf(args...)
#endif

#if defined(DEBUG_PRINT) || defined(DEBUG_CHECKS)
#define ASSERT(cond, fmt...) \
	if (!(cond)) { apk_error(fmt); *(char*)NULL = 0; }
#else
#define ASSERT(cond, fmt...)
#endif

#define SCORE_ZERO		(struct apk_score) { .unsatisfied = 0 }
#define SCORE_MAX		(struct apk_score) { .unsatisfied = -1 }
#define SCORE_FMT		"{%d/%d/%d,%d}"
#define SCORE_PRINTF(s)		(s)->unsatisfied, (s)->non_preferred_actions, (s)->non_preferred_pinnings, (s)->preference

enum {
	DECISION_ASSIGN = 0,
	DECISION_EXCLUDE
};

enum {
	BRANCH_NO,
	BRANCH_YES,
};

struct apk_decision {
	struct apk_name *name;
	union {
		struct apk_package *pkg;
		unsigned backup_until;
	};
#ifdef DEBUG_CHECKS
	struct apk_score saved_score;
	unsigned short saved_requirers;
#endif
	unsigned short requirers;
	unsigned type : 1;
	unsigned has_package : 1;
	unsigned branching_point : 1;
	unsigned topology_position : 1;
	unsigned found_solution : 1;
};

struct apk_package_state {
	/* set on startup */
	unsigned int topology_soft;
	unsigned short allowed_pinning_maybe;

	/* dynamic */
	unsigned short allowed_pinning;
	unsigned short inherited_pinning[APK_MAX_TAGS];
	unsigned short inherited_upgrade;
	unsigned short inherited_reinstall;

	unsigned short conflicts;
	unsigned short unsatisfied;

	unsigned char preference;
	unsigned handle_install_if : 1;
	unsigned allowed : 1;
	unsigned locked : 1;

	unsigned solver_flags_maybe : 4;
};

#define CHOSEN_NONE	(struct apk_provider) { .pkg = NULL, .version = NULL }

struct apk_solver_state {
	struct apk_database *db;
	struct apk_decision *decisions;

	struct list_head unsolved_list_head;

	unsigned int num_topology_positions;
	unsigned int num_decisions, max_decisions;
	unsigned int topology_position;
	unsigned int assigned_names;

	struct apk_solution_array *best_solution;

	struct apk_score score;
	struct apk_score best_score;
	struct apk_score minimum_penalty;

	unsigned solver_flags : 4;
	unsigned impossible_state : 1;
};

typedef enum {
	SOLVERR_SOLUTION = 0,
	SOLVERR_PRUNED,
	SOLVERR_EXPAND,
	SOLVERR_STOP,
} solver_result_t;

static void apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static void undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep);
static solver_result_t push_decision(struct apk_solver_state *ss,
				     struct apk_name *name,
				     struct apk_package *pkg,
				     int primary_decision,
				     int branching_point,
				     int topology_position);

#ifdef DEBUG_CHECKS
static void addscore(struct apk_score *a, struct apk_score *b)
{
	struct apk_score orig = *a;
	a->score += b->score;
	ASSERT(a->unsatisfied >= orig.unsatisfied, "Unsatisfied overflow");
	ASSERT(a->non_preferred_actions >= orig.non_preferred_actions, "Preferred action overflow");
	ASSERT(a->non_preferred_pinnings >= orig.non_preferred_pinnings, "Preferred pinning overflow");
	ASSERT(a->preference >= orig.preference, "Preference overflow");
}

static void subscore(struct apk_score *a, struct apk_score *b)
{
	struct apk_score orig = *a;
	a->score -= b->score;
	ASSERT(a->unsatisfied <= orig.unsatisfied, "Unsatisfied underflow");
	ASSERT(a->non_preferred_actions <= orig.non_preferred_actions, "Preferred action underflow");
	ASSERT(a->non_preferred_pinnings <= orig.non_preferred_pinnings, "Preferred pinning overflow");
	ASSERT(a->preference <= orig.preference, "Preference underflow");
}
#else
static void addscore(struct apk_score *a, struct apk_score *b)
{
	a->score += b->score;
}

static void subscore(struct apk_score *a, struct apk_score *b)
{
	a->score -= b->score;
}
#endif

static inline int cmpscore(struct apk_score *a, struct apk_score *b)
{
	if (a->score < b->score) return -1;
	if (a->score > b->score) return 1;
	return 0;
}

static inline int cmpscore2(struct apk_score *a1, struct apk_score *a2, struct apk_score *b)
{
	struct apk_score a;
	a.score = a1->score + a2->score;
	if (a.score < b->score) return -1;
	if (a.score > b->score) return 1;
	return 0;
}

static struct apk_package_state *pkg_to_ps(struct apk_package *pkg)
{
	return (struct apk_package_state*) pkg->state_ptr;
}

static struct apk_package_state *pkg_to_ps_alloc(struct apk_package *pkg)
{
	if (pkg->state_ptr == NULL)
		pkg->state_ptr = calloc(1, sizeof(struct apk_package_state));
	return pkg_to_ps(pkg);
}

static inline struct apk_package *decision_to_pkg(struct apk_decision *d)
{
	return d->has_package ? d->pkg : NULL;
}

static inline int pkg_available(struct apk_database *db, struct apk_package *pkg)
{
	/* virtual packages - only deps used; no real .apk */
	if (pkg->installed_size == 0)
		return TRUE;
	/* obviously present */
	if (pkg->in_cache || pkg->filename != NULL || (pkg->repos & db->local_repos))
		return TRUE;
	/* can download */
	if ((pkg->repos & ~db->bad_repos) && !(apk_flags & APK_NO_NETWORK))
		return TRUE;
	return FALSE;
}

static void foreach_dependency_pkg(
	struct apk_solver_state *ss, struct apk_package *parent_package, struct apk_dependency_array *depends,
	void (*cb)(struct apk_solver_state *ss, struct apk_package *dependency, struct apk_package *parent_package))
{
	int i, j;

	/* And sort the main dependencies */
	for (i = 0; i < depends->num; i++) {
		struct apk_dependency *dep = &depends->item[i];
		struct apk_name *name0 = dep->name;

		for (j = 0; j < name0->providers->num; j++) {
			struct apk_provider *p0 = &name0->providers->item[j];

			/* conflict depends on all to be not installed */
			if (!apk_dep_is_provided(dep, p0))
				continue;

			cb(ss, p0->pkg, parent_package);
		}
	}
}

static void foreach_rinstall_if_pkg(
	struct apk_solver_state *ss, struct apk_package *pkg,
	void (*cb)(struct apk_solver_state *ss, struct apk_package *rinstall_if, struct apk_package *parent_pkg))
{
	struct apk_name *name = pkg->name;
	int i, j, k;

	for (i = 0; i < pkg->name->rinstall_if->num; i++) {
		struct apk_name *name0 = pkg->name->rinstall_if->item[i];

		dbg_printf(PKG_VER_FMT ": rinstall_if %s\n",
			   PKG_VER_PRINTF(pkg), name0->name);

		for (j = 0; j < name0->providers->num; j++) {
			struct apk_provider *p0 = &name0->providers->item[j];

			for (k = 0; k < p0->pkg->install_if->num; k++) {
				struct apk_dependency *dep = &p0->pkg->install_if->item[k];

				if (dep->name == name &&
				    apk_dep_is_provided(dep, p0))
					break;
			}
			if (k >= p0->pkg->install_if->num)
				continue;

			/* pkg depends (via install_if) on pkg0 */
			cb(ss, p0->pkg, pkg);
		}
	}
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

static int get_topology_score(
		struct apk_solver_state *ss,
		struct apk_name *name,
		struct apk_package *pkg,
		struct apk_score *_score)
{
	struct apk_package_state *ps = pkg_to_ps(pkg);
	struct apk_score score;
	unsigned int repos;
	unsigned short preferred_pinning, allowed_pinning;
	unsigned int preferred_repos, allowed_repos;
	int score_locked = TRUE, sticky_installed = FALSE;

	score = (struct apk_score) {
		.unsatisfied = ps->unsatisfied,
		.preference = ps->preference,
	};

	if (ss->solver_flags & APK_SOLVERF_AVAILABLE) {
		/* available preferred */
		if ((pkg->repos == 0) && name->ss.has_available_pkgs)
			score.non_preferred_actions++;
	} else if (ps->inherited_reinstall ||
		   (((name->ss.solver_flags_local|ss->solver_flags) & APK_SOLVERF_REINSTALL))) {
		/* reinstall requested, but not available */
		if (!pkg_available(ss->db, pkg))
			score.non_preferred_actions++;
	} else if (ps->inherited_upgrade ||
		   ((name->ss.solver_flags_local|ss->solver_flags) & APK_SOLVERF_UPGRADE)) {
		/* upgrading - score is just locked here */
	} else if ((ps->inherited_upgrade == 0) &&
		   ((name->ss.solver_flags_local|ss->solver_flags) & APK_SOLVERF_UPGRADE) == 0 &&
		   ((ps->solver_flags_maybe & APK_SOLVERF_UPGRADE) == 0 || (ps->locked))) {
		/* not upgrading: it is not preferred to change package */
		if (pkg->ipkg == NULL && name->ss.originally_installed)
			score.non_preferred_actions++;
		else
			sticky_installed = TRUE;
	} else {
		score_locked = FALSE;
	}

	repos = pkg->repos | (pkg->ipkg ? ss->db->repo_tags[pkg->ipkg->repository_tag].allowed_repos : 0);
	preferred_pinning = name->ss.preferred_pinning ?: APK_DEFAULT_PINNING_MASK;
	preferred_repos = get_pinning_mask_repos(ss->db, preferred_pinning);

	if (!(repos & preferred_repos))
		score.non_preferred_pinnings++;

	if (ps->locked || (ps->allowed_pinning | ps->allowed_pinning_maybe) == ps->allowed_pinning) {
		allowed_pinning = ps->allowed_pinning | preferred_pinning | APK_DEFAULT_PINNING_MASK;
		allowed_repos = get_pinning_mask_repos(ss->db, allowed_pinning);
		if (!(repos & allowed_repos)) {
			if (sticky_installed)
				score.non_preferred_actions++;
			score.non_preferred_pinnings += 16;
		}
	} else {
		score_locked = FALSE;
	}

	*_score = score;
	return score_locked;
}

static int compare_absolute_package_preference(
		struct apk_provider *pA,
		struct apk_provider *pB)
{
	struct apk_package *pkgA = pA->pkg;
	struct apk_package *pkgB = pB->pkg;

	/* specified on command line directly */
	if (pkgA->filename && !pkgB->filename)
		return 1;
	if (pkgB->filename && !pkgA->filename)
		return -1;

	/* upgrading, or neither of the package is installed, so
	 * we just fall back comparing to versions */
	switch (apk_version_compare_blob(*pA->version, *pB->version)) {
	case APK_VERSION_GREATER:
		return 1;
	case APK_VERSION_LESS:
		return -1;
	}

	/* prefer the installed package */
	if (pkgA->ipkg != NULL && pkgB->ipkg == NULL)
		return 1;
	if (pkgB->ipkg != NULL && pkgA->ipkg == NULL)
		return -1;

	/* prefer the one with lowest available repository */
	return ffsl(pkgB->repos) - ffsl(pkgA->repos);
}

static void calculate_pkg_preference(struct apk_package *pkg)
{
	struct apk_name *name = pkg->name;
	struct apk_package_state *ps = pkg_to_ps(pkg);
	struct apk_provider p = APK_PROVIDER_FROM_PACKAGE(pkg);
	int i, j;

	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		if (pkg == p0->pkg)
			continue;
		if (compare_absolute_package_preference(&p, p0) < 0)
			ps->preference++;
	}
	for (i = 0; i < pkg->provides->num; i++) {
		struct apk_dependency *d0 = &pkg->provides->item[i];
		if (d0->version == &apk_null_blob)
			continue;
		for (j = 0; j < d0->name->providers->num; j++) {
			struct apk_provider *p0 = &d0->name->providers->item[j];
			if (name == p0->pkg->name)
				continue;
			if (compare_absolute_package_preference(&p, p0) < 0)
				ps->preference++;
		}
	}

	dbg_printf(PKG_VER_FMT ": preference=%d\n", PKG_VER_PRINTF(pkg), ps->preference);
}

static void count_name(struct apk_solver_state *ss, struct apk_name *name)
{
	if (!name->ss.decision_counted) {
		ss->max_decisions++;
		name->ss.decision_counted = 1;
	}
}

static void sort_hard_dependencies(struct apk_solver_state *ss,
				   struct apk_package *pkg,
				   struct apk_package *parent_pkg)
{
	struct apk_name *name = pkg->name;
	struct apk_package_state *ps;
	int i;

	ps = pkg_to_ps_alloc(pkg);
	if (ps->topology_soft)
		return;
	pkg->topology_hard = -1;
	ps->topology_soft = -1;

	calculate_pkg_preference(pkg);

	/* Consider hard dependencies only */
	foreach_dependency_pkg(ss, pkg, pkg->depends, sort_hard_dependencies);
	foreach_dependency_pkg(ss, pkg, pkg->install_if, sort_hard_dependencies);

	ss->max_decisions++;
	name->ss.has_available_pkgs = 1;
	count_name(ss, name);
	for (i = 0; i < pkg->provides->num; i++) {
		pkg->provides->item[i].name->ss.has_available_pkgs = 1;
		count_name(ss, pkg->provides->item[i].name);
	}

	ps->topology_soft = pkg->topology_hard = ++ss->num_topology_positions;
	dbg_printf(PKG_VER_FMT ": topology_hard=%d\n",
		   PKG_VER_PRINTF(pkg), pkg->topology_hard);
}

static void sort_soft_dependencies(struct apk_solver_state *ss,
				   struct apk_package *pkg,
				   struct apk_package *parent_pkg)
{
	struct apk_package_state *ps;
	struct apk_package_state *parent_ps;

	sort_hard_dependencies(ss, pkg, parent_pkg);

	ps = pkg_to_ps(pkg);
	if (ps->topology_soft != pkg->topology_hard)
		return;
	ps->topology_soft = -1;

	/* Update state */
	parent_ps = pkg_to_ps(parent_pkg);
	ps->allowed_pinning_maybe |= parent_ps->allowed_pinning_maybe;
	ps->solver_flags_maybe |= parent_ps->solver_flags_maybe;

	/* Soft reverse dependencies aka. install_if */
	foreach_rinstall_if_pkg(ss, pkg, sort_hard_dependencies);
	foreach_dependency_pkg(ss, pkg, pkg->depends, sort_soft_dependencies);

	/* Assign a topology sorting order */
	ps->topology_soft = ++ss->num_topology_positions;
	dbg_printf(PKG_VER_FMT ": topology_soft=%d\n",
		   PKG_VER_PRINTF(pkg), ps->topology_soft);
}

static void update_allowed(struct apk_database *db, struct apk_package *pkg)
{
	struct apk_package_state *ps = pkg_to_ps(pkg);
	unsigned int repos;

	repos = pkg->repos | (pkg->ipkg ? db->repo_tags[pkg->ipkg->repository_tag].allowed_repos : 0);
	if (repos & get_pinning_mask_repos(db, ps->allowed_pinning | APK_DEFAULT_PINNING_MASK))
		ps->allowed = 1;
	else
		ps->allowed = 0;
}

static void sort_name(struct apk_solver_state *ss, struct apk_name *name)
{
	int i, j;

	for (i = 0; i < name->providers->num; i++) {
		struct apk_package *pkg = name->providers->item[i].pkg;
		struct apk_package_state *ps = pkg_to_ps_alloc(pkg);
		unsigned short allowed_pinning;

		ps->allowed_pinning |= name->ss.preferred_pinning;
		ps->allowed_pinning_maybe |= name->ss.preferred_pinning;

		allowed_pinning = ps->allowed_pinning;
		for (j = 0; allowed_pinning; j++) {
			if (!(allowed_pinning & BIT(j)))
				continue;
			allowed_pinning &= ~BIT(j);
			ps->inherited_pinning[j]++;
		}
		update_allowed(ss->db, pkg);
		sort_soft_dependencies(ss, name->providers->item[i].pkg, pkg);
	}
}

static void foreach_dependency(struct apk_solver_state *ss, struct apk_dependency_array *deps,
			       void (*func)(struct apk_solver_state *ss, struct apk_dependency *dep))
{
	int i;
	for (i = 0; i < deps->num; i++)
		func(ss, &deps->item[i]);
}

static int install_if_missing(struct apk_solver_state *ss, struct apk_package *pkg, struct apk_name *exclude)
{
	int i, missing = 0;

	for (i = 0; i < pkg->install_if->num; i++) {
		struct apk_dependency *dep = &pkg->install_if->item[i];
		struct apk_name *name = dep->name;

		if (name == exclude ||
		    !name->ss.locked || !apk_dep_is_provided(dep, &name->ss.chosen))
			missing++;
	}

	return missing;
}

static void get_unassigned_score(struct apk_name *name, struct apk_score *score)
{
	*score = (struct apk_score){
		.unsatisfied = name->ss.requirers,
		.preference = name->providers->num,
	};
}

static void promote_name(struct apk_solver_state *ss, struct apk_name *name)
{
	if (name->ss.locked)
		return;

	/* queue for handling if needed */
	if (!list_hashed(&name->ss.unsolved_list))
		list_add_tail(&name->ss.unsolved_list, &ss->unsolved_list_head);

	/* update info, but no cached information flush is required, as
	 * minimum_penalty can only go up */
	name->ss.name_touched = 1;
}

static void demote_name(struct apk_solver_state *ss, struct apk_name *name)
{
	if (name->ss.locked)
		return;

	/* remove cached information */
	name->ss.chosen = CHOSEN_NONE;

	/* and remove list, or request refresh */
	if (name->ss.requirers == 0 && name->ss.install_ifs == 0) {
		if (list_hashed(&name->ss.unsolved_list)) {
			list_del(&name->ss.unsolved_list);
			list_init(&name->ss.unsolved_list);
			dbg_printf("%s: not required\n", name->name);
		}
	} else {
		/* still needed, put back to list */
		promote_name(ss, name);
	}
}

static int inherit_package_state(struct apk_database *db, struct apk_package *to, struct apk_package *from)
{
	struct apk_name *fname = from->name;
	struct apk_package_state *tps = pkg_to_ps(to);
	struct apk_package_state *fps = pkg_to_ps(from);
	int i, changed = 0;

	if ((fname->ss.solver_flags_inheritable & APK_SOLVERF_REINSTALL) ||
	    fps->inherited_reinstall) {
		tps->inherited_reinstall++;
		changed = 1;
	}

	if ((fname->ss.solver_flags_inheritable & APK_SOLVERF_UPGRADE) ||
	    fps->inherited_upgrade) {
		tps->inherited_upgrade++;
		changed = 1;
	}

	if (fps->allowed_pinning) {
		unsigned short allowed_pinning = fps->allowed_pinning;
		for (i = 0; allowed_pinning && i < db->num_repo_tags; i++) {
			if (!(allowed_pinning & BIT(i)))
				continue;
			allowed_pinning &= ~BIT(i);
			if (tps->inherited_pinning[i]++ == 0) {
				tps->allowed_pinning |= BIT(i);
				changed = 2;
			}
		}
		if (changed == 2)
			update_allowed(db, to);
	}

	return changed;
}

static void uninherit_package_state(struct apk_database *db, struct apk_package *to, struct apk_package *from)
{
	struct apk_name *fname = from->name;
	struct apk_package_state *tps = pkg_to_ps(to);
	struct apk_package_state *fps = pkg_to_ps(from);
	int i, changed = 0;

	if ((fname->ss.solver_flags_inheritable & APK_SOLVERF_REINSTALL) ||
	    fps->inherited_reinstall)
		tps->inherited_reinstall--;

	if ((fname->ss.solver_flags_inheritable & APK_SOLVERF_UPGRADE) ||
	    fps->inherited_upgrade)
		tps->inherited_upgrade--;

	if (fps->allowed_pinning) {
		unsigned short allowed_pinning = fps->allowed_pinning;
		for (i = 0; allowed_pinning && i < db->num_repo_tags; i++) {
			if (!(allowed_pinning & BIT(i)))
				continue;
			allowed_pinning &= ~BIT(i);
			if (--tps->inherited_pinning[i] == 0) {
				tps->allowed_pinning &= ~BIT(i);
				changed = 2;
			}
		}
		if (changed == 2)
			update_allowed(db, to);
	}
}

static void trigger_install_if(struct apk_solver_state *ss,
			       struct apk_package *pkg,
			       struct apk_package *parent_pkg)
{
	struct apk_name *name = pkg->name;

	if (install_if_missing(ss, pkg, NULL) != 0)
		return;

	dbg_printf("trigger_install_if: " PKG_VER_FMT " triggered\n",
		   PKG_VER_PRINTF(pkg));
	name->ss.install_ifs++;
	inherit_package_state(ss->db, pkg, parent_pkg);
	promote_name(ss, name);
}

static void untrigger_install_if(struct apk_solver_state *ss,
			         struct apk_package *pkg,
			         struct apk_package *parent_pkg)
{
	struct apk_name *name = pkg->name;

	if (install_if_missing(ss, pkg, parent_pkg->name) != 0)
		return;

	dbg_printf("untrigger_install_if: " PKG_VER_FMT " no longer triggered\n",
		   PKG_VER_PRINTF(pkg));
	name->ss.install_ifs--;
	uninherit_package_state(ss->db, pkg, parent_pkg);
	demote_name(ss, name);
}

static inline void assign_name(
	struct apk_solver_state *ss, struct apk_name *name, struct apk_provider p)
{
	if (name->ss.locked &&
	    (p.version != &apk_null_blob || name->ss.chosen.version != &apk_null_blob)) {
		/* Assigning locked name with version is a problem;
		 * generally package providing same name twice */
		name->ss.locked++;
		ss->impossible_state = 1;
		return;
	}
	if (!name->ss.locked) {
		struct apk_package *pkg = p.pkg;

		subscore(&ss->minimum_penalty, &name->ss.minimum_penalty);
		name->ss.minimum_penalty = SCORE_ZERO;
		name->ss.chosen = p;

		if (name->ss.requirers) {
			struct apk_score score;
			if (pkg != NULL)
				get_topology_score(ss, name, pkg, &score);
			else
				get_unassigned_score(name, &score);
			addscore(&ss->score, &score);
		}
	}
	name->ss.locked++;
	if (list_hashed(&name->ss.unsolved_list)) {
		list_del(&name->ss.unsolved_list);
		list_init(&name->ss.unsolved_list);
	}
}

static inline void unassign_name(struct apk_solver_state *ss, struct apk_name *name)
{
	ASSERT(name->ss.locked, "Unassigning unlocked name %s", name->name);
	name->ss.locked--;
	if (name->ss.locked == 0) {
		struct apk_package *pkg = name->ss.chosen.pkg;

		name->ss.chosen = CHOSEN_NONE;
		name->ss.name_touched = 1;
		demote_name(ss, name);

		if (name->ss.requirers) {
			struct apk_score score;
			if (pkg != NULL)
				get_topology_score(ss, name, pkg, &score);
			else
				get_unassigned_score(name, &score);
			subscore(&ss->score, &score);
		}
	}
}

static solver_result_t apply_decision(struct apk_solver_state *ss,
				      struct apk_decision *d)
{
	struct apk_name *name = d->name;
	struct apk_package *pkg = decision_to_pkg(d);
	int i;

	ss->impossible_state = 0;
	name->ss.name_touched = 1;
	if (pkg != NULL) {
		struct apk_package_state *ps = pkg_to_ps(pkg);

		dbg_printf("-->apply_decision: " PKG_VER_FMT " %s\n",
			   PKG_VER_PRINTF(pkg),
			   (d->type == DECISION_ASSIGN) ? "ASSIGN" : "EXCLUDE");

		for (i = 0; i < pkg->provides->num; i++)
			pkg->provides->item[i].name->ss.name_touched = 1;

		ps->locked = 1;

		if (d->type == DECISION_ASSIGN &&
		    ps->topology_soft < ss->topology_position) {
			ps->handle_install_if = 1;
			dbg_printf("triggers due to " PKG_VER_FMT "\n",
				   PKG_VER_PRINTF(pkg));
		} else {
			ps->handle_install_if = 0;
		}

		if (d->topology_position) {
			if (ps->topology_soft < ss->topology_position)
				ss->topology_position = ps->topology_soft;
			else
				ss->topology_position = pkg->topology_hard;
		}

		if (d->type == DECISION_ASSIGN) {
			ss->assigned_names++;
			assign_name(ss, name, APK_PROVIDER_FROM_PACKAGE(pkg));
			for (i = 0; i < pkg->provides->num; i++) {
				struct apk_dependency *p = &pkg->provides->item[i];
				assign_name(ss, p->name, APK_PROVIDER_FROM_PROVIDES(pkg, p));
			}

			foreach_dependency(ss, pkg->depends, apply_constraint);
			if (ps->handle_install_if)
				foreach_rinstall_if_pkg(ss, pkg, trigger_install_if);
		}
	} else {
		dbg_printf("-->apply_decision: %s %s NOTHING\n",
			   name->name,
			   (d->type == DECISION_ASSIGN) ? "ASSIGN" : "EXCLUDE");

		if (d->type == DECISION_ASSIGN) {
			assign_name(ss, name, CHOSEN_NONE);
		} else {
			name->ss.none_excluded = 1;
		}
	}

	if (ss->impossible_state) {
		dbg_printf("%s: %s impossible constraints\n",
			name->name,
			(d->type == DECISION_ASSIGN) ? "ASSIGN" : "EXCLUDE");
		return SOLVERR_PRUNED;
	}

	if (cmpscore2(&ss->score, &ss->minimum_penalty, &ss->best_score) >= 0) {
		dbg_printf("%s: %s penalty too big: "SCORE_FMT"+"SCORE_FMT">="SCORE_FMT"\n",
			name->name,
			(d->type == DECISION_ASSIGN) ? "ASSIGN" : "EXCLUDE",
			SCORE_PRINTF(&ss->score),
			SCORE_PRINTF(&ss->minimum_penalty),
			SCORE_PRINTF(&ss->best_score));
		return SOLVERR_PRUNED;
	}

	return SOLVERR_EXPAND;
}

static void undo_decision(struct apk_solver_state *ss,
			  struct apk_decision *d)
{
	struct apk_name *name = d->name;
	struct apk_package *pkg = decision_to_pkg(d);
	int i;

	name->ss.name_touched = 1;

	if (pkg != NULL) {
		struct apk_package_state *ps = pkg_to_ps(pkg);

		dbg_printf("-->undo_decision: " PKG_VER_FMT " %s\n",
			   PKG_VER_PRINTF(pkg),
			   (d->type == DECISION_ASSIGN) ? "ASSIGN" : "EXCLUDE");

		if (d->topology_position) {
			if (ps->handle_install_if)
				ss->topology_position = ps->topology_soft;
			else
				ss->topology_position = pkg->topology_hard;
		}

		for (i = 0; i < pkg->provides->num; i++)
			pkg->provides->item[i].name->ss.name_touched = 1;

		if (d->type == DECISION_ASSIGN) {
			if (ps->handle_install_if)
				foreach_rinstall_if_pkg(ss, pkg, untrigger_install_if);
			foreach_dependency(ss, pkg->depends, undo_constraint);

			unassign_name(ss, pkg->name);
			for (i = 0; i < pkg->provides->num; i++) {
				struct apk_dependency *p = &pkg->provides->item[i];
				unassign_name(ss, p->name);
			}
			ss->assigned_names--;
		}
		ps->locked = 0;
	} else {
		dbg_printf("-->undo_decision: %s %s NOTHING\n",
			   name->name,
			   (d->type == DECISION_ASSIGN) ? "ASSIGN" : "EXCLUDE");

		if (d->type == DECISION_ASSIGN) {
			unassign_name(ss, name);
		} else {
			name->ss.none_excluded = 0;
		}

		/* Put back the name to unsolved list */
		promote_name(ss, name);
	}
}

static solver_result_t push_decision(struct apk_solver_state *ss,
				     struct apk_name *name,
				     struct apk_package *pkg,
				     int primary_decision,
				     int branching_point,
				     int topology_position)
{
	struct apk_decision *d;
	int i;

	ASSERT(ss->num_decisions <= ss->max_decisions,
	       "Decision tree overflow.");

	ss->num_decisions++;
	d = &ss->decisions[ss->num_decisions];

#ifdef DEBUG_CHECKS
	dbg_printf("Saving score ("SCORE_FMT") and requirers (%d) for %s\n",
		SCORE_PRINTF(&ss->score), name->ss.requirers, name->name);
	d->saved_score = ss->score;
	d->saved_requirers = name->ss.requirers;
#endif
	d->type = primary_decision;
	d->branching_point = branching_point;
	d->topology_position = topology_position;
	d->found_solution = 0;
	d->name = name;
	d->requirers = name->ss.requirers;
	if (pkg) {
		d->has_package = 1;
		d->pkg = pkg;
		for (i = 0; i < pkg->provides->num; i++)
			d->requirers += pkg->provides->item[i].name->ss.requirers;
	} else {
		d->has_package = 0;
		d->backup_until = name->ss.last_touched_decision;
	}

	return apply_decision(ss, d);
}

static int next_branch(struct apk_solver_state *ss)
{
	unsigned int backup_until = ss->num_decisions;

	while (ss->num_decisions > 0) {
		struct apk_decision *d = &ss->decisions[ss->num_decisions];
		struct apk_name *name = d->name;

		undo_decision(ss, d);

#ifdef DEBUG_CHECKS
		ASSERT(cmpscore(&d->saved_score, &ss->score) == 0,
			"Saved_score "SCORE_FMT" != score "SCORE_FMT,
			SCORE_PRINTF(&d->saved_score),
			SCORE_PRINTF(&ss->score));
		ASSERT(d->saved_requirers == name->ss.requirers,
			"Requirers not restored between decisions (%s), %d != %d",
			name->name, d->saved_requirers, name->ss.requirers);
#endif

		if (backup_until >= ss->num_decisions &&
		    d->branching_point == BRANCH_YES) {
			d->branching_point = BRANCH_NO;
			d->type = (d->type == DECISION_ASSIGN) ? DECISION_EXCLUDE : DECISION_ASSIGN;
			return apply_decision(ss, d);
		} else if (d->branching_point == BRANCH_YES) {
			dbg_printf("skipping %s, %d < %d\n",
				name->name, backup_until, ss->num_decisions);
		}

		/* When undoing the initial "exclude none" decision, check if
		 * we can backjump. */
		if (d->has_package == 0 && !d->found_solution) {
			if (d->backup_until && d->backup_until < backup_until) {
				backup_until = d->backup_until;
				/* We can't backtrack over the immediate
				 * EXCLUDE decisions, as they are in a sense
				 * part of the bundle. */
				while (backup_until < ss->num_decisions &&
				       !ss->decisions[backup_until+1].has_package)
					backup_until++;
			}
		}

		ss->num_decisions--;
	}

	dbg_printf("-->next_branch: no more branches\n");
	return SOLVERR_STOP;
}

static void apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_decision *d = &ss->decisions[ss->num_decisions];
	struct apk_package *requirer_pkg = decision_to_pkg(d);
	struct apk_name *name = dep->name;
	int i, changed = 0, strength = d->requirers;

	dbg_printf("--->apply_constraint: %s (strength %d)\n", name->name, strength);

	if (name->ss.locked) {
		if (name->ss.chosen.pkg)
			dbg_printf("%s: locked to " PKG_VER_FMT " already\n",
				   name->name, PKG_VER_PRINTF(name->ss.chosen.pkg));
		else
			dbg_printf("%s: locked to empty\n",
				   name->name);
		if (!apk_dep_is_provided(dep, &name->ss.chosen)) {
			dbg_printf("%s: constraint violation %d\n",
				   name->name, strength);
			ss->score.unsatisfied += strength;
			if (dep->conflict)
				ss->impossible_state = 1;
		}
		return;
	}

	if (name->providers->num == 0) {
		if (!dep->conflict)
			ss->score.unsatisfied += strength;
		return;
	}

	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		struct apk_package *pkg0 = p0->pkg;
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0 == NULL || ps0->locked ||
		    ss->topology_position < pkg0->topology_hard)
			continue;

		if (!apk_dep_is_provided(dep, p0)) {
			if (dep->conflict)
				ps0->conflicts++;
			else
				ps0->unsatisfied++;

			dbg_printf(PKG_VER_FMT ": conflicts++ -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
			changed |= 1;
		} else if (requirer_pkg != NULL) {
			dbg_printf(PKG_VER_FMT ": inheriting flags and pinning from "PKG_VER_FMT"\n",
				   PKG_VER_PRINTF(pkg0),
				   PKG_VER_PRINTF(requirer_pkg));
			changed |= inherit_package_state(ss->db, pkg0, requirer_pkg);
		}
	}

	if (name->ss.last_touched_decision == 0 || changed) {
		dep->solver_state = name->ss.last_touched_decision + 1;
		name->ss.last_touched_decision = ss->num_decisions;
	}

	if (!dep->conflict) {
		dbg_printf("%s requirers += %d\n", name->name, strength);
		name->ss.requirers += strength;
	}

	promote_name(ss, name);
}

static void undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_decision *d = &ss->decisions[ss->num_decisions];
	struct apk_name *name = dep->name;
	struct apk_package *requirer_pkg = decision_to_pkg(d);
	int i, strength = d->requirers;

	dbg_printf("--->undo_constraint: %s (strength %d)\n", name->name, strength);

	if (name->ss.locked) {
		if (name->ss.chosen.pkg != NULL) {
			dbg_printf(PKG_VER_FMT " selected already for %s\n",
				   PKG_VER_PRINTF(name->ss.chosen.pkg), name->name);
		} else {
			dbg_printf("%s selected to not be satisfied\n",
				   name->name);
		}
		if (!apk_dep_is_provided(dep, &name->ss.chosen))
			ss->score.unsatisfied -= strength;
		return;
	}
	if (name->providers->num == 0) {
		if (!dep->conflict)
			ss->score.unsatisfied -= strength;
		return;
	}

	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		struct apk_package *pkg0 = p0->pkg;
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0 == NULL || ps0->locked ||
		    ss->topology_position < pkg0->topology_hard)
			continue;

		if (!apk_dep_is_provided(dep, p0)) {
			if (dep->conflict)
				ps0->conflicts--;
			else
				ps0->unsatisfied--;
			dbg_printf(PKG_VER_FMT ": conflicts-- -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
		} else if (requirer_pkg != NULL) {
			dbg_printf(PKG_VER_FMT ": uninheriting flags and pinning from "PKG_VER_FMT"\n",
				   PKG_VER_PRINTF(pkg0),
				   PKG_VER_PRINTF(requirer_pkg));
			uninherit_package_state(ss->db, pkg0, requirer_pkg);
		}
	}

	if (dep->solver_state) {
		name->ss.last_touched_decision = dep->solver_state - 1;
		dep->solver_state = 0;
	}

	if (!dep->conflict) {
		name->ss.requirers -= strength;
		dbg_printf("%s requirers -= %d\n", name->name, strength);
	}

	demote_name(ss, name);
}

static int reconsider_name(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_provider *next_p = NULL, *best_p = NULL;
	unsigned int next_topology = 0, options = 0;
	int i, j, score_locked = FALSE;
	struct apk_score best_score = SCORE_MAX;
	struct apk_score ss_score;

	subscore(&ss->minimum_penalty, &name->ss.minimum_penalty);
	name->ss.minimum_penalty = SCORE_ZERO;

	ss_score = ss->score;
	addscore(&ss_score, &ss->minimum_penalty);

	if (!name->ss.none_excluded) {
		struct apk_score minscore;
		get_unassigned_score(name, &minscore);
		if (cmpscore2(&ss_score, &minscore, &ss->best_score) >= 0) {
			dbg_printf("%s: pruning none, score too high "SCORE_FMT"+"SCORE_FMT">="SCORE_FMT"\n",
				name->name,
				SCORE_PRINTF(&ss_score),
				SCORE_PRINTF(&minscore),
				SCORE_PRINTF(&ss->best_score));
			return push_decision(ss, name, NULL, DECISION_EXCLUDE, BRANCH_NO, FALSE);
		}

		return push_decision(ss, name, NULL, DECISION_EXCLUDE, BRANCH_YES, FALSE);
	}

	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		struct apk_package *pkg0 = p0->pkg;
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);
		struct apk_score pkg0_score;

		if (ps0 == NULL || ps0->locked || ps0->conflicts ||
		    ss->topology_position < pkg0->topology_hard ||
		    (pkg0->ipkg == NULL && pkg0->filename == NULL &&
		     (!ps0->allowed || !pkg_available(ss->db, pkg0))))
			continue;

		for (j = 0; j < pkg0->provides->num; j++) {
			struct apk_dependency *p00 = &pkg0->provides->item[j];
			if (!p00->name->ss.locked)
				continue;
			if (p00->name->ss.chosen.version != &apk_null_blob ||
			    p00->version != &apk_null_blob) {
				dbg_printf("reconsider_name: "PKG_VER_FMT": pruning due to provides (%s) conflict\n",
					   PKG_VER_PRINTF(pkg0), p00->name->name);
				return push_decision(ss, name, pkg0, DECISION_EXCLUDE, BRANCH_NO, FALSE);
			}
		}

		score_locked = get_topology_score(ss, name, pkg0, &pkg0_score);

		/* viable alternative? */
		if (cmpscore2(&ss_score, &pkg0_score, &ss->best_score) >= 0) {
			dbg_printf("reconsider_name: "PKG_VER_FMT": pruning due to score "SCORE_FMT"+"SCORE_FMT">="SCORE_FMT"\n",
				   PKG_VER_PRINTF(pkg0),
				   SCORE_PRINTF(&ss_score), SCORE_PRINTF(&pkg0_score),
				   SCORE_PRINTF(&ss->best_score));
			return push_decision(ss, name, pkg0, DECISION_EXCLUDE, BRANCH_NO, FALSE);
		}

		if (cmpscore(&pkg0_score, &best_score) < 0) {
			best_score = pkg0_score;
			best_p = p0;
		}

		/* next in topology order - next to get locked in */
		if (ps0->topology_soft < ss->topology_position &&
		    ps0->topology_soft > next_topology)
			next_p = p0, next_topology = ps0->topology_soft;
		else if (pkg0->topology_hard > next_topology)
			next_p = p0, next_topology = pkg0->topology_hard;

		options++;
	}

	/* no options left */
	if (options == 0) {
		if (name->ss.none_excluded) {
			dbg_printf("reconsider_name: %s: no options pruning branch\n",
				name->name);
			return SOLVERR_PRUNED;
		}
		dbg_printf("reconsider_name: %s: no options locking none\n",
			name->name);
		return push_decision(ss, name, NULL, DECISION_ASSIGN, BRANCH_NO, FALSE);
	} else if (options == 1 && score_locked && name->ss.none_excluded && name == next_p->pkg->name) {
		dbg_printf("reconsider_name: %s: only one choice left with known score, locking.\n",
			name->name);
		return push_decision(ss, name, next_p->pkg, DECISION_ASSIGN, BRANCH_NO, FALSE);
	}

	name->ss.chosen = *next_p;
	name->ss.preferred_chosen = (best_p == next_p);
	name->ss.minimum_penalty = best_score;
	addscore(&ss->minimum_penalty, &best_score);
	for (i = 0; i < best_p->pkg->provides->num; i++) {
		struct apk_dependency *p = &best_p->pkg->provides->item[i];
		get_topology_score(ss, p->name, best_p->pkg, &best_score);
		addscore(&ss->minimum_penalty, &best_score);
	}

	dbg_printf("reconsider_name: %s: next_pkg="PKG_VER_FMT", preferred_chosen=%d\n",
		name->name, PKG_VER_PRINTF(next_p->pkg), name->ss.preferred_chosen);

	return SOLVERR_SOLUTION;
}

static int expand_branch(struct apk_solver_state *ss)
{
	struct apk_name *name0 = NULL, *name;
	struct apk_package *pkg0 = NULL;
	struct apk_package_state *ps0;
	unsigned int r, topology0 = 0;
	int primary_decision, branching_point;
	int can_install = FALSE;

	list_for_each_entry(name, &ss->unsolved_list_head, ss.unsolved_list) {
		struct apk_package *cpkg;
		struct apk_package_state *cps;

		if (name->ss.name_touched) {
			dbg_printf("%s: reconsidering things\n",
				   name->name);
			r = reconsider_name(ss, name);
			if (r != SOLVERR_SOLUTION)
				return r;
			name->ss.name_touched = 0;
		}
		cpkg = name->ss.chosen.pkg;
		cps  = pkg_to_ps(cpkg);
		if (cps->topology_soft < ss->topology_position &&
		    cps->topology_soft >= topology0) {
			topology0 = cps->topology_soft;
		} else if (cpkg->topology_hard >= topology0) {
			topology0 = cpkg->topology_hard;
		} else {
			continue;
		}
		if (pkg0 != cpkg) {
			can_install = FALSE;
			name0 = name;
		}
		pkg0 = cpkg;
		if (name->ss.chosen.version != &apk_null_blob) {
			can_install |= TRUE;
			name0 = name;
		}
	}
	if (name0 == NULL || pkg0 == NULL) {
		dbg_printf("expand_branch: solution with score "SCORE_FMT"\n",
			   SCORE_PRINTF(&ss->score));
		return SOLVERR_SOLUTION;
	}

	/* someone needs to provide this name -- find next eligible
	 * provider candidate */
	ps0 = pkg_to_ps(pkg0);
	name = name0;

	if (!name->ss.none_excluded) {
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);
		if (ps0->unsatisfied > name->ss.requirers)
			primary_decision = DECISION_ASSIGN;
		else
			primary_decision = DECISION_EXCLUDE;
		return push_decision(ss, name, NULL, primary_decision, BRANCH_YES, FALSE);
	}

	if (!can_install) {
		/* provides with no version; not eglible to auto install */
		dbg_printf("expand_branch: "PKG_VER_FMT" not installable. excluding.",
			   PKG_VER_PRINTF(pkg0));
		return push_decision(ss, pkg0->name, pkg0, DECISION_EXCLUDE, BRANCH_NO, TRUE);
	}

	dbg_printf("expand_branch: "PKG_VER_FMT" score: "SCORE_FMT"+"SCORE_FMT"\tbest: "SCORE_FMT"\n",
		PKG_VER_PRINTF(pkg0),
		SCORE_PRINTF(&ss->score),
		SCORE_PRINTF(&ss->minimum_penalty),
		SCORE_PRINTF(&ss->best_score));

	if (!ps0->allowed) {
		/* pinning has not enabled the package */
		primary_decision = DECISION_EXCLUDE;
		/* but if it is installed, we might consider it */
		if ((pkg0->ipkg == NULL) && (pkg0->filename == NULL))
			branching_point = BRANCH_NO;
		else
			branching_point = BRANCH_YES;
	} else if (name->ss.requirers == 0 && name->ss.install_ifs != 0 &&
		   install_if_missing(ss, pkg0, NULL)) {
		/* not directly required, and package specific
		 * install_if never triggered */
		primary_decision = DECISION_EXCLUDE;
		branching_point = BRANCH_NO;
	} else if (name->ss.preferred_chosen) {
		primary_decision = DECISION_ASSIGN;
		branching_point = BRANCH_YES;
	} else {
		primary_decision = DECISION_EXCLUDE;
		branching_point = BRANCH_YES;
	}

	return push_decision(ss, pkg0->name, pkg0,
			     primary_decision, branching_point, TRUE);
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
	int i, n;

	apk_solution_array_resize(&ss->best_solution, ss->assigned_names);

	n = 0;
	for (i = ss->num_decisions; i > 0; i--) {
		struct apk_decision *d = &ss->decisions[i];
		struct apk_name *name = d->name;
		struct apk_package *pkg = decision_to_pkg(d);
		struct apk_package_state *ps;
		unsigned short pinning;
		unsigned int repos;

		d->found_solution = 1;

		if (pkg == NULL) {
			dbg_printf("record_solution: %s: NOTHING\n", name->name);
			continue;
		}

		dbg_printf("record_solution: " PKG_VER_FMT ": %s\n",
			   PKG_VER_PRINTF(pkg),
			   d->type == DECISION_ASSIGN ? "INSTALL" : "no install");

		if (d->type != DECISION_ASSIGN)
			continue;

		ps = pkg_to_ps(pkg);
		pinning = ps->allowed_pinning | name->ss.preferred_pinning | APK_DEFAULT_PINNING_MASK;
		repos = pkg->repos | (pkg->ipkg ? db->repo_tags[pkg->ipkg->repository_tag].allowed_repos : 0);

		ASSERT(n < ss->assigned_names, "Name assignment overflow");
		ss->best_solution->item[n++] = (struct apk_solution_entry){
			.pkg = pkg,
			.reinstall = ps->inherited_reinstall ||
				((name->ss.solver_flags_local | ss->solver_flags) & APK_SOLVERF_REINSTALL),
			.repository_tag = get_tag(db, pinning, repos),
		};
	}
	apk_solution_array_resize(&ss->best_solution, n);
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
	struct apk_package *pkg;
	struct apk_installed_package *ipkg;
	int i, num_installs = 0, num_removed = 0, ci = 0;

	/* calculate change set size */
	for (i = 0; i < solution->num; i++) {
		pkg = solution->item[i].pkg;
		name = pkg->name;
		name->ss.chosen = APK_PROVIDER_FROM_PACKAGE(pkg);
		name->ss.in_changeset = 1;
		if ((pkg->ipkg == NULL) ||
		    solution->item[i].reinstall ||
		    solution->item[i].repository_tag != pkg->ipkg->repository_tag)
			num_installs++;
	}
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		name = ipkg->pkg->name;
		if ((name->ss.chosen.pkg == NULL) || !name->ss.in_changeset)
			num_removed++;
	}

	/* construct changeset */
	apk_change_array_resize(&changeset->changes, num_installs + num_removed);
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		name = ipkg->pkg->name;
		if ((name->ss.chosen.pkg == NULL) || !name->ss.in_changeset) {
			changeset->changes->item[ci].oldpkg = ipkg->pkg;
			ci++;
		}
	}
	for (i = 0; i < solution->num; i++) {
		pkg = solution->item[i].pkg;
		name = pkg->name;

		if ((pkg->ipkg == NULL) ||
		    solution->item[i].reinstall ||
		    solution->item[i].repository_tag != pkg->ipkg->repository_tag){
			changeset->changes->item[ci].oldpkg = apk_pkg_get_installed(name);
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

	ASSERT(name->ss.requirers == 0, "Requirers is not zero after cleanup");
	memset(&name->ss, 0, sizeof(name->ss));

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
	name->ss.solver_flags_local = solver_flags;
	name->ss.solver_flags_inheritable = solver_flags & solver_flags_inheritable;
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
	solver_result_t r = SOLVERR_STOP;
	int i;

	ss = calloc(1, sizeof(struct apk_solver_state));
	ss->db = db;
	ss->solver_flags = solver_flags;
	ss->topology_position = -1;
	ss->best_score = SCORE_MAX;
	list_init(&ss->unsolved_list_head);

	for (i = 0; i < world->num; i++) {
		struct apk_dependency *dep = &world->item[i];
		struct apk_name *name = dep->name;

		dbg_printf("%s: adding pinnings %d\n", name->name, dep->repository_tag);
		name->ss.in_world_dependency = 1;
		name->ss.preferred_pinning = BIT(dep->repository_tag);

		sort_name(ss, name);
	}
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		sort_name(ss, ipkg->pkg->name);
		ipkg->pkg->name->ss.originally_installed = 1;
	}

	ss->max_decisions ++;
	ss->decisions = calloc(1, sizeof(struct apk_decision[ss->max_decisions]));

	/* "Initial decision" is used as dummy for world constraints. */
	ss->decisions[0].requirers = 1;
	foreach_dependency(ss, world, apply_constraint);

	do {
		/* need EXPAND if here, can return SOLUTION|PRUNED|EXPAND */
		r = expand_branch(ss);
		if (r == SOLVERR_SOLUTION) {
			if (cmpscore(&ss->score, &ss->best_score) < 0) {
				dbg_printf("updating best score "SCORE_FMT" (was: "SCORE_FMT")\n",
					SCORE_PRINTF(&ss->score),
					SCORE_PRINTF(&ss->best_score));

				record_solution(ss);
				ss->best_score = ss->score;
			}

			r = SOLVERR_PRUNED;
		}
		/* next_branch() returns PRUNED, STOP or EXPAND */
		while (r == SOLVERR_PRUNED)
			r = next_branch(ss);
		/* STOP or EXPAND */
	} while (r != SOLVERR_STOP);

#ifdef DEBUG_CHECKS
	foreach_dependency(ss, world, undo_constraint);
#endif


	if (ss->best_solution == NULL) {
		apk_error("Internal error: no solution at all found.");
		return -1;
	}

	/* collect packages */
	dbg_printf("finished. best score "SCORE_FMT". solution has %zu packages.\n",
		SCORE_PRINTF(&ss->best_score),
		ss->best_solution->num);

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
	i = ss->best_score.unsatisfied;
	apk_solver_free(db);
	free(ss->decisions);
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
	apk_blob_t *oneversion = NULL;
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
		msg = "Installing";
		oneversion = newpkg->version;
	} else if (newpkg == NULL) {
		msg = "Purging";
		oneversion = oldpkg->version;
	} else if (newpkg == oldpkg) {
		if (change->reinstall) {
			if (pkg_available(db, change->newpkg))
				msg = "Re-installing";
			else
				msg = "[APK unavailable, skipped] Re-installing";
		} else {
			msg = "Updating pinning";
		}
		oneversion = newpkg->version;
	} else {
		r = apk_pkg_version_compare(newpkg, oldpkg);
		switch (r) {
		case APK_VERSION_LESS:
			msg = "Downgrading";
			break;
		case APK_VERSION_EQUAL:
			msg = "Replacing";
			break;
		case APK_VERSION_GREATER:
			msg = "Upgrading";
			break;
		}
	}
	if (oneversion) {
		apk_message("%s %s %s (" BLOB_FMT ")",
			    status, msg, nameptr,
			    BLOB_PRINTF(*oneversion));
	} else {
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
	size_t percent;
	int progress_fd;
};

static void update_progress(struct progress *prog, size_t percent)
{
	if (prog->percent == percent)
		return;
	prog->percent = percent;
	if (apk_flags & APK_PROGRESS)
		draw_progress(percent);
	if (prog->progress_fd != 0) {
		char buf[8];
		size_t n = snprintf(buf, sizeof(buf), "%zu\n", percent);
		write(prog->progress_fd, buf, n);
	}
}

static void progress_cb(void *ctx, size_t pkg_percent)
{
	struct progress *prog = (struct progress *) ctx;
	size_t partial = 0, percent;

	if (prog->pkg != NULL)
		partial = muldiv(pkg_percent, prog->pkg->installed_size, APK_PROGRESS_SCALE);
	percent = muldiv(100, prog->done.bytes + prog->done.packages + partial,
			 prog->total.bytes + prog->total.packages);
	update_progress(prog, percent);
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
	int i, r = 0, size_diff = 0, size_unit;

	if (apk_db_check_world(db, world) != 0) {
		apk_error("Not committing changes due to missing repository tags. Use --force to override.");
		return -1;
	}

	if (changeset->changes == NULL)
		goto all_done;

	/* Count what needs to be done */
	memset(&prog, 0, sizeof(prog));
	prog.progress_fd = db->progress_fd;
	for (i = 0; i < changeset->changes->num; i++) {
		change = &changeset->changes->item[i];
		count_change(change, &prog.total);
		if (change->newpkg)
			size_diff += change->newpkg->installed_size;
		if (change->oldpkg)
			size_diff -= change->oldpkg->installed_size;
	}
	size_diff /= 1024;
	size_unit = 'K';
	if (abs(size_diff) > 10000) {
		size_diff /= 1024;
		size_unit = 'M';
	}

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
			printf("After this operation, %d %ciB of %s\n",
				abs(size_diff), size_unit,
				(size_diff < 0) ?
				"disk space will be freed." :
				"additional disk space will be used.");
		}
		if (changeset->changes->num > 0 &&
		    (apk_flags & APK_INTERACTIVE)) {
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
		prog.pkg = change->newpkg;
		progress_cb(&prog, 0);

		if (!(apk_flags & APK_SIMULATE)) {
			if (change->oldpkg != change->newpkg ||
			    (change->reinstall && pkg_available(db, change->newpkg)))
				r = apk_db_install_pkg(db,
						       change->oldpkg, change->newpkg,
						       progress_cb, &prog);
			if (r != 0)
				break;
			if (change->newpkg)
				change->newpkg->ipkg->repository_tag = change->repository_tag;
		}

		count_change(change, &prog.done);
	}
	update_progress(&prog, 100);

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

static void add_name_if_virtual(struct apk_name_array **names, struct apk_name *name)
{
	int i;

	if (name->state_int == 1)
		return;
	name->state_int = 1;

	if (name->providers->num == 0)
		return;

	for (i = 0; i < name->providers->num; i++)
		if (name->providers->item[i].pkg->name == name)
			return;

	*apk_name_array_add(names) = name;
}

static void print_dep_errors(struct apk_database *db, char *label,
			     struct apk_dependency_array *deps,
			     struct apk_name_array **names)
{
	int i, print_label = 1;
	char buf[256];
	apk_blob_t p;
	struct apk_indent indent;

	for (i = 0; i < deps->num; i++) {
		struct apk_dependency *dep = &deps->item[i];
		struct apk_package *pkg = NULL;

		if (dep->name->state_int != 1)
			pkg = (struct apk_package*) dep->name->state_ptr;

		if (apk_dep_is_materialized_or_provided(dep, pkg))
			continue;

		if (pkg == NULL)
			add_name_if_virtual(names, dep->name);

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
	struct apk_name_array *names;
	char pkgtext[256];
	int i, j;

	apk_name_array_init(&names);
	apk_error("%d unsatisfiable dependencies:", unsatisfiable);

	for (i = 0; i < solution->num; i++) {
		struct apk_package *pkg = solution->item[i].pkg;
		pkg->name->state_ptr = pkg;
		for (j = 0; j < pkg->provides->num; j++) {
			if (pkg->provides->item[j].version == &apk_null_blob)
				continue;
			pkg->provides->item[j].name->state_ptr = pkg;
		}
	}

	print_dep_errors(db, "world", world, &names);
	for (i = 0; i < solution->num; i++) {
		struct apk_package *pkg = solution->item[i].pkg;
		snprintf(pkgtext, sizeof(pkgtext), PKG_VER_FMT, PKG_VER_PRINTF(pkg));
		print_dep_errors(db, pkgtext, pkg->depends, &names);
	}

	for (i = 0; i < names->num; i++) {
		struct apk_indent indent;
		struct apk_name *name = names->item[i];

		printf("\n  %s is a virtual package provided by:\n    ", name->name);
		indent.x = 4;
		indent.indent = 4;

		for (j = 0; j < name->providers->num; j++) {
			struct apk_package *pkg = name->providers->item[j].pkg;
			snprintf(pkgtext, sizeof(pkgtext), PKG_VER_FMT, PKG_VER_PRINTF(pkg));
			apk_print_indented(&indent, APK_BLOB_STR(pkgtext));
		}
		printf("\n");
	}
	apk_name_array_free(&names);
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
