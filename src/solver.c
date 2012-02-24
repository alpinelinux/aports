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
#include "apk_defines.h"
#include "apk_database.h"
#include "apk_package.h"
#include "apk_solver.h"

#include "apk_print.h"

//#define DEBUG_PRINT
//#define DEBUG_CHECKS

#ifdef DEBUG_PRINT
#include <stdio.h>
#define dbg_printf(args...) fprintf(stderr, args)
#else
#define dbg_printf(args...)
#endif

#if defined(DEBUG_PRINT) || defined(DEBUG_CHECKS)
#define ASSERT(cond, fmt...) \
	if (!(cond)) { fprintf(stderr, fmt); *(char*)NULL = 0; }
#else
#define ASSERT(cond, fmt...)
#endif

struct apk_score {
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			unsigned short preference;
			unsigned short non_preferred_pinnings;
			unsigned short non_preferred_actions;
			unsigned short conflicts;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			unsigned short conflicts;
			unsigned short non_preferred_actions;
			unsigned short non_preferred_pinnings;
			unsigned short preference;
#else
#error Unknown endianess.
#endif
		};
		uint64_t score;
	};
};

#define SCORE_FMT		"{%d/%d/%d,%d}"
#define SCORE_PRINTF(s)		(s)->conflicts, (s)->non_preferred_actions, (s)->non_preferred_pinnings, (s)->preference

enum {
	DECISION_ASSIGN = 0,
	DECISION_EXCLUDE
};

enum {
	BRANCH_NO,
	BRANCH_YES,
};

struct apk_decision {
	union {
		struct apk_name *name;
		struct apk_package *pkg;
	};
#ifdef DEBUG_CHECKS
	struct apk_score saved_score;
	unsigned short saved_requirers;
#endif

	unsigned no_package : 1;
	unsigned type : 1;
	unsigned branching_point : 1;
	unsigned topology_position : 1;
	unsigned found_solution : 1;
};

struct apk_package_state {
	unsigned int topology_soft;
	unsigned short conflicts;
	unsigned char preference;
	unsigned handle_install_if : 1;
	unsigned locked : 1;
};

#define CHOSEN_NONE	(struct apk_provider) { .pkg = NULL, .version = NULL }

struct apk_name_state {
	struct list_head unsolved_list;
	struct apk_name *name;
	struct apk_provider chosen;

	struct apk_score minimum_penalty;
	unsigned short requirers;
	unsigned short install_ifs;

	/* set on startup */
	unsigned short preferred_pinning;
	unsigned short maybe_pinning;

	/* dynamic */
	unsigned int last_touched_decision;
	unsigned short allowed_pinning;
	unsigned short inherited_pinning[APK_MAX_TAGS];
	unsigned short inherited_upgrade;
	unsigned short inherited_reinstall;

	/* one time prepare/finish flags */
	unsigned solver_flags_local : 4;
	unsigned solver_flags_local_mask : 4;
	unsigned solver_flags_maybe : 4;

	unsigned decision_counted : 1;
	unsigned originally_installed : 1;
	unsigned has_available_pkgs : 1;
	unsigned in_changeset : 1;
	unsigned in_world_dependency : 1;

	/* dynamic state flags */
	unsigned none_excluded : 1;
	unsigned locked : 1;
	unsigned name_touched : 1;
};

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
	struct apk_score minimum_penalty;
	struct apk_score best_score;

	unsigned solver_flags : 4;
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
	ASSERT(a->conflicts >= orig.conflicts, "Conflict overflow");
	ASSERT(a->non_preferred_actions >= orig.non_preferred_actions, "Preferred action overflow");
	ASSERT(a->preference >= orig.preference, "Preference overflow");
}

static void subscore(struct apk_score *a, struct apk_score *b)
{
	struct apk_score orig = *a;
	a->score -= b->score;
	ASSERT(a->conflicts <= orig.conflicts, "Conflict underflow");
	ASSERT(a->non_preferred_actions <= orig.non_preferred_actions, "Preferred action underflow");
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

static struct apk_name *decision_to_name(struct apk_decision *d)
{
	if (d->no_package)
		return d->name;
	return d->pkg->name;
}

static struct apk_package *decision_to_pkg(struct apk_decision *d)
{
	if (d->no_package)
		return NULL;
	return d->pkg;
}

static struct apk_package_state *pkg_to_ps(struct apk_package *pkg)
{
	return (struct apk_package_state*) pkg->state_ptr;
}

static struct apk_name_state *name_to_ns(struct apk_name *name)
{
	return (struct apk_name_state*) name->state_ptr;
}

static struct apk_name_state *name_to_ns_alloc(struct apk_name *name)
{
	struct apk_name_state *ns;
	int i;

	if (name->state_ptr == NULL) {
		ns = calloc(1, sizeof(struct apk_name_state));
		ns->name = name;
		for (i = 0; i < name->providers->num; i++) {
			if (name->providers->item[i].pkg->repos != 0) {
				ns->has_available_pkgs = 1;
				break;
			}
		}
		name->state_ptr = ns;
	} else {
		ns = (struct apk_name_state*) name->state_ptr;
	}
	return ns;
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
	struct apk_solver_state *ss, struct apk_dependency_array *depends,
	void (*cb)(struct apk_solver_state *ss, struct apk_package *dependency))
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

			cb(ss, p0->pkg);
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
			cb(ss, p0->pkg);
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
		struct apk_name_state *ns,
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
		.conflicts = ps->conflicts,
		.preference = ps->preference,
	};

	if (ss->solver_flags & APK_SOLVERF_AVAILABLE) {
		/* available preferred */
		if ((pkg->repos == 0) && ns->has_available_pkgs)
			score.non_preferred_actions++;
	} else if (ns->inherited_reinstall ||
		   (((ns->solver_flags_local|ss->solver_flags) & APK_SOLVERF_REINSTALL))) {
		/* reinstall requested, but not available */
		if (!pkg_available(ss->db, pkg))
			score.non_preferred_actions++;
	} else if (ns->inherited_upgrade ||
		   ((ns->solver_flags_local|ss->solver_flags) & APK_SOLVERF_UPGRADE)) {
		/* upgrading - score is just locked here */
	} else if ((ns->inherited_upgrade == 0) &&
		   ((ns->solver_flags_local|ss->solver_flags) & APK_SOLVERF_UPGRADE) == 0 &&
		   ((ns->solver_flags_maybe & APK_SOLVERF_UPGRADE) == 0 || (ps->locked))) {
		/* not upgrading: it is not preferred to change package */
		if (pkg->ipkg == NULL && ns->originally_installed)
			score.non_preferred_actions++;
		else
			sticky_installed = TRUE;
	} else {
		score_locked = FALSE;
	}

	repos = pkg->repos | (pkg->ipkg ? ss->db->repo_tags[pkg->ipkg->repository_tag].allowed_repos : 0);
	preferred_pinning = ns->preferred_pinning ?: APK_DEFAULT_PINNING_MASK;
	preferred_repos = get_pinning_mask_repos(ss->db, preferred_pinning);

	if (!(repos & preferred_repos))
		score.non_preferred_pinnings++;

	if (ns->locked || (ns->allowed_pinning | ns->maybe_pinning) == ns->allowed_pinning) {
		allowed_pinning = ns->allowed_pinning | preferred_pinning | APK_DEFAULT_PINNING_MASK;
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

static int is_topology_optimum(struct apk_solver_state *ss,
			       struct apk_package *pkg)
{
	struct apk_name *name = pkg->name;
	struct apk_name_state *ns = name_to_ns(name);
	struct apk_score score;
	int i;

	get_topology_score(ss, ns, pkg, &score);
	for (i = 0; i < name->providers->num; i++) {
		struct apk_package *pkg0 = name->providers->item[i].pkg;
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);
		struct apk_score score0;

		if (pkg0 == pkg)
			continue;

		if (ps0 == NULL || ps0->locked ||
		    ss->topology_position < pkg->topology_hard)
			continue;

		get_topology_score(ss, ns, pkg0, &score0);
		if (cmpscore(&score0, &score) < 0)
			return 0;
	}

	return 1;
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
	int i;

	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		if (pkg == p0->pkg)
			continue;
		if (compare_absolute_package_preference(&p, p0) < 0)
			ps->preference++;
	}
	/* FIXME: consider all provided names too */
}

static void count_name(struct apk_solver_state *ss, struct apk_name_state *ns)
{
	if (!ns->decision_counted) {
		ss->max_decisions++;
		ns->decision_counted = 1;
	}
}

static void sort_hard_dependencies(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_package_state *ps;
	struct apk_name_state *ns;

	if (pkg->state_ptr == NULL)
		pkg->state_ptr = calloc(1, sizeof(struct apk_package_state));

	ps = pkg_to_ps(pkg);
	if (ps->topology_soft)
		return;
	pkg->topology_hard = -1;
	ps->topology_soft = -1;

	calculate_pkg_preference(pkg);
	/* Consider hard dependencies only */
	foreach_dependency_pkg(ss, pkg->depends, sort_hard_dependencies);
	foreach_dependency_pkg(ss, pkg->install_if, sort_hard_dependencies);

	ss->max_decisions++;
	ns = name_to_ns_alloc(pkg->name);
	count_name(ss, ns);

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

static void recalculate_maybe(struct apk_solver_state *ss, struct apk_name *name,
			      unsigned short flags, unsigned short pinning)
{
	struct apk_name_state *ns = name_to_ns_alloc(name);
	int propagate = FALSE;
	int i, j;

	if ((ns->maybe_pinning & pinning) != pinning) {
		ns->maybe_pinning |= pinning;
		propagate = TRUE;
	}
	if ((ns->solver_flags_maybe & flags) != flags) {
		ns->solver_flags_maybe |= flags;
		propagate = TRUE;
	}
	if (!propagate)
		return;

	for (i = 0; i < name->providers->num; i++) {
		struct apk_package *pkg = name->providers->item[i].pkg;

		for (j = 0; j < pkg->depends->num; j++) {
			struct apk_dependency *dep = &pkg->depends->item[j];
			struct apk_name *name0 = dep->name;
			recalculate_maybe(ss, name0, flags, pinning);
		}
	}

	for (i = 0; i < name->rinstall_if->num; i++) {
		struct apk_name *name0 = name->rinstall_if->item[i];
		recalculate_maybe(ss, name0, flags, pinning);
	}
}

static void sort_name(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name_state *ns = name_to_ns_alloc(name);
	int i;

	for (i = 0; i < name->providers->num; i++)
		sort_soft_dependencies(ss, name->providers->item[i].pkg);

	count_name(ss, ns);
	recalculate_maybe(ss, name,
			  ns->solver_flags_local & ns->solver_flags_local_mask,
			  ns->maybe_pinning);
}

static void foreach_dependency(struct apk_solver_state *ss, struct apk_dependency_array *deps,
			       void (*func)(struct apk_solver_state *ss, struct apk_dependency *dep))
{
	int i;
	for (i = 0; i < deps->num; i++)
		func(ss, &deps->item[i]);
}

static int install_if_missing(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_name_state *ns;
	int i, missing = 0;

	for (i = 0; i < pkg->install_if->num; i++) {
		struct apk_dependency *dep = &pkg->install_if->item[i];

		ns = name_to_ns(dep->name);

		/* ns can be NULL, if the install_if has a name with
		 * no packages */
		if (ns == NULL || !ns->locked ||
		    !apk_dep_is_provided(dep, &ns->chosen))
			missing++;
	}

	return missing;
}

static void get_unassigned_score(struct apk_name *name, struct apk_score *score)
{
	struct apk_name_state *ns = name_to_ns(name);

	*score = (struct apk_score){
		.conflicts = ns->requirers,
		.preference = name->providers->num,
	};
}

static void promote_name(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name_state *ns = name_to_ns(name);

	if (ns->locked)
		return;

	/* queue for handling if needed */
	if (!list_hashed(&ns->unsolved_list))
		list_add_tail(&ns->unsolved_list, &ss->unsolved_list_head);

	/* update info, but no cached information flush is required, as
	 * minimum_penalty can only go up */
	ns->name_touched = 1;
}

static void demote_name(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name_state *ns = name_to_ns(name);

	if (ns->locked)
		return;

	/* remove cached information */
	subscore(&ss->minimum_penalty, &ns->minimum_penalty);
	ns->minimum_penalty = (struct apk_score) { .score = 0 };
	ns->chosen = CHOSEN_NONE;

	/* and remove list, or request refresh */
	if (ns->requirers == 0 && ns->install_ifs == 0) {
		if (list_hashed(&ns->unsolved_list)) {
			list_del(&ns->unsolved_list);
			list_init(&ns->unsolved_list);
			dbg_printf("%s: not required\n", name->name);
		}
	} else {
		ns->name_touched = 1;
	}
}

static void inherit_name_state(struct apk_database *db, struct apk_name *to, struct apk_name *from)
{
	struct apk_name_state *tns = name_to_ns(to);
	struct apk_name_state *fns = name_to_ns(from);
	int i;

	if ((fns->solver_flags_local & fns->solver_flags_local_mask & APK_SOLVERF_REINSTALL) ||
	    fns->inherited_reinstall)
		tns->inherited_reinstall++;

	if ((fns->solver_flags_local & fns->solver_flags_local_mask & APK_SOLVERF_UPGRADE) ||
	    fns->inherited_upgrade)
		tns->inherited_upgrade++;

	if (fns->allowed_pinning) {
		for (i = 0; i < db->num_repo_tags; i++) {
			if (!(fns->allowed_pinning & BIT(i)))
				continue;
			if (tns->inherited_pinning[i]++ == 0)
				tns->allowed_pinning |= BIT(i);
		}
	}
}

static void uninherit_name_state(struct apk_database *db, struct apk_name *to, struct apk_name *from)
{
	struct apk_name_state *tns = name_to_ns(to);
	struct apk_name_state *fns = name_to_ns(from);
	int i;

	if ((fns->solver_flags_local & fns->solver_flags_local_mask & APK_SOLVERF_REINSTALL) ||
	    fns->inherited_reinstall)
		tns->inherited_reinstall--;

	if ((fns->solver_flags_local & fns->solver_flags_local_mask & APK_SOLVERF_UPGRADE) ||
	    fns->inherited_upgrade)
		tns->inherited_upgrade--;

	if (fns->allowed_pinning) {
		for (i = 0; i < db->num_repo_tags; i++) {
			if (!(fns->allowed_pinning & BIT(i)))
				continue;
			if (--tns->inherited_pinning[i] == 0)
				tns->allowed_pinning &= ~BIT(i);
		}
	}
}

static void trigger_install_if(struct apk_solver_state *ss,
			       struct apk_package *pkg)
{
	if (install_if_missing(ss, pkg) == 0) {
		struct apk_name *name0 = decision_to_name(&ss->decisions[ss->num_decisions]);
		struct apk_name_state *ns = name_to_ns(pkg->name);

		dbg_printf("trigger_install_if: " PKG_VER_FMT " triggered\n",
			   PKG_VER_PRINTF(pkg));
		ns->install_ifs++;
		inherit_name_state(ss->db, pkg->name, name0);
		promote_name(ss, pkg->name);
	}
}

static void untrigger_install_if(struct apk_solver_state *ss,
			       struct apk_package *pkg)
{
	if (install_if_missing(ss, pkg) != 1) {
		struct apk_name *name0 = decision_to_name(&ss->decisions[ss->num_decisions]);
		struct apk_name_state *ns = name_to_ns(pkg->name);

		dbg_printf("untrigger_install_if: " PKG_VER_FMT " no longer triggered\n",
			   PKG_VER_PRINTF(pkg));
		ns->install_ifs--;
		uninherit_name_state(ss->db, pkg->name, name0);
		demote_name(ss, pkg->name);
	}
}

static solver_result_t apply_decision(struct apk_solver_state *ss,
				      struct apk_decision *d)
{
	struct apk_name *name = decision_to_name(d);
	struct apk_name_state *ns = name_to_ns(name);
	struct apk_package *pkg = decision_to_pkg(d);
	struct apk_score score;

	ns->name_touched = 1;
	if (pkg != NULL) {
		struct apk_package_state *ps = pkg_to_ps(pkg);

		dbg_printf("-->apply_decision: " PKG_VER_FMT " %s\n",
			   PKG_VER_PRINTF(pkg),
			   (d->type == DECISION_ASSIGN) ? "ASSIGN" : "EXCLUDE");

		ps->locked = 1;
		ps->handle_install_if = 0;

		if (d->topology_position) {
			if (ps->topology_soft < ss->topology_position) {
				if (d->type == DECISION_ASSIGN) {
					ps->handle_install_if = 1;
					dbg_printf("triggers due to " PKG_VER_FMT "\n",
						   PKG_VER_PRINTF(pkg));
				}
				ss->topology_position = ps->topology_soft;
			} else {
				ss->topology_position = pkg->topology_hard;
			}
		}

		if (d->type == DECISION_ASSIGN) {
			subscore(&ss->minimum_penalty, &ns->minimum_penalty);
			ns->minimum_penalty = (struct apk_score) { .score = 0 };

			ns->locked = 1;
			get_topology_score(ss, ns, pkg, &score);
			addscore(&ss->score, &score);

			if (cmpscore2(&ss->score, &ss->minimum_penalty, &ss->best_score) >= 0) {
				dbg_printf("install causing "SCORE_FMT", penalty too big: "SCORE_FMT"+"SCORE_FMT">="SCORE_FMT"\n",
					   SCORE_PRINTF(&score),
					   SCORE_PRINTF(&ss->score),
					   SCORE_PRINTF(&ss->minimum_penalty),
					   SCORE_PRINTF(&ss->best_score));

				subscore(&ss->score, &score);
				ns->locked = 0;

				return SOLVERR_PRUNED;
			}

			ss->assigned_names++;
			ns->chosen = APK_PROVIDER_FROM_PACKAGE(pkg);

			list_del(&ns->unsolved_list);
			list_init(&ns->unsolved_list);

			foreach_dependency(ss, pkg->depends, apply_constraint);
			foreach_rinstall_if_pkg(ss, pkg, trigger_install_if);
		}
	} else {
		dbg_printf("-->apply_decision: %s %s NOTHING\n",
			   name->name,
			   (d->type == DECISION_ASSIGN) ? "ASSIGN" : "EXCLUDE");

		if (d->type == DECISION_ASSIGN) {
			subscore(&ss->minimum_penalty, &ns->minimum_penalty);
			ns->minimum_penalty = (struct apk_score) { .score = 0 };

			get_unassigned_score(name, &score);
			addscore(&ss->score, &score);

			ns->chosen = CHOSEN_NONE;
			ns->locked = 1;
			list_del(&ns->unsolved_list);
			list_init(&ns->unsolved_list);
		} else {
			ns->none_excluded = 1;
		}
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
	struct apk_name *name = decision_to_name(d);
	struct apk_name_state *ns = name_to_ns(name);
	struct apk_package *pkg = decision_to_pkg(d);
	struct apk_score score;

	ns->name_touched = 1;

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

		if (ns->locked) {
			ss->assigned_names--;
			foreach_rinstall_if_pkg(ss, pkg, untrigger_install_if);
			foreach_dependency(ss, pkg->depends, undo_constraint);

			get_topology_score(ss, ns, pkg, &score);
			subscore(&ss->score, &score);
		}
		ps->locked = 0;
	} else {
		dbg_printf("-->undo_decision: %s %s NOTHING\n",
			   name->name,
			   (d->type == DECISION_ASSIGN) ? "ASSIGN" : "EXCLUDE");

		if (d->type == DECISION_ASSIGN) {
			get_unassigned_score(name, &score);
			subscore(&ss->score, &score);
		} else {
			ns->none_excluded = 0;
		}
	}

	ns->locked = 0;
	ns->chosen = CHOSEN_NONE;

	/* Put back the name to unsolved list */
	promote_name(ss, name);
}

static solver_result_t push_decision(struct apk_solver_state *ss,
				     struct apk_name *name,
				     struct apk_package *pkg,
				     int primary_decision,
				     int branching_point,
				     int topology_position)
{
	struct apk_decision *d;

	ASSERT(ss->num_decisions <= ss->max_decisions,
	       "Decision tree overflow.\n");

	ss->num_decisions++;
	d = &ss->decisions[ss->num_decisions];

#ifdef DEBUG_CHECKS
	d->saved_score = ss->score;
	d->saved_requirers = name_to_ns(name)->requirers;
#endif
	d->type = primary_decision;
	d->branching_point = branching_point;
	d->topology_position = topology_position;
	d->found_solution = 0;
	if (pkg == NULL) {
		d->name = name;
		d->no_package = 1;
	} else {
		d->pkg = pkg;
		d->no_package = 0;
	}

	return apply_decision(ss, d);
}

static int next_branch(struct apk_solver_state *ss)
{
	unsigned int backup_until = ss->num_decisions;

	while (ss->num_decisions > 0) {
		struct apk_decision *d = &ss->decisions[ss->num_decisions];
		struct apk_name *name = decision_to_name(d);
		struct apk_name_state *ns = name_to_ns(name);

		undo_decision(ss, d);

#ifdef DEBUG_CHECKS
		ASSERT(cmpscore(&d->saved_score, &ss->score) == 0,
			"ERROR! saved_score "SCORE_FMT" != score "SCORE_FMT"\n",
			SCORE_PRINTF(&d->saved_score),
			SCORE_PRINTF(&ss->score));
		ASSERT(d->saved_requirers == ns->requirers,
			"ERROR! requirers not restored between decisions\n");
#endif

		if (backup_until >= ss->num_decisions &&
		    d->branching_point == BRANCH_YES) {
			d->branching_point = BRANCH_NO;
			d->type = (d->type == DECISION_ASSIGN) ? DECISION_EXCLUDE : DECISION_ASSIGN;
			return apply_decision(ss, d);
		}

		if (d->no_package && !d->found_solution) {
			if (ns->last_touched_decision < backup_until)
				backup_until = ns->last_touched_decision;
		}

		ss->num_decisions--;
	}

	dbg_printf("-->next_branch: no more branches\n");
	return SOLVERR_STOP;
}

static void apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_state *ns = name_to_ns(name);
	int i, strength;

	if (ss->num_decisions > 0) {
		struct apk_name *name0 = decision_to_name(&ss->decisions[ss->num_decisions]);
		struct apk_name_state *ns0 = name_to_ns(name0);
		strength = ns0->requirers ?: 1;
	} else {
		strength = 1;
	}

	if (ns->locked) {
		if (ns->chosen.pkg)
			dbg_printf("%s: locked to " PKG_VER_FMT " already\n",
				   name->name, PKG_VER_PRINTF(ns->chosen.pkg));
		else
			dbg_printf("%s: locked to empty\n",
				   name->name);
		if (!apk_dep_is_provided(dep, &ns->chosen))
			ss->score.conflicts += strength;
		return;
	}
	if (name->providers->num == 0) {
		if (!dep->optional)
			ss->score.conflicts += strength;
		return;
	}

	if (dep->repository_tag) {
		dbg_printf("%s: adding pinnings %d\n",
			   dep->name->name, dep->repository_tag);
		ns->preferred_pinning = BIT(dep->repository_tag);
		ns->allowed_pinning |= BIT(dep->repository_tag);
		ns->inherited_pinning[dep->repository_tag]++;
		recalculate_maybe(ss, name, 0, ns->allowed_pinning);
	}

	if (ss->num_decisions > 0) {
		struct apk_name *name0 = decision_to_name(&ss->decisions[ss->num_decisions]);
		dbg_printf("%s: inheriting flags and pinning from %s\n",
			   name->name, name0->name);
		inherit_name_state(ss->db, name, name0);
		ns->last_touched_decision = ss->num_decisions;
	}

	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		struct apk_package *pkg0 = p0->pkg;
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0 == NULL || ps0->locked ||
		    ss->topology_position < pkg0->topology_hard)
			continue;

		if (!apk_dep_is_provided(dep, p0)) {
			ps0->conflicts++;
			dbg_printf(PKG_VER_FMT ": conflicts++ -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
		}
	}

	if (!dep->optional)
		ns->requirers += strength;

	promote_name(ss, name);
}

static void undo_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_state *ns = name_to_ns(name);
	int i, strength;

	if (ss->num_decisions > 0) {
		struct apk_name *name0 = decision_to_name(&ss->decisions[ss->num_decisions]);
		struct apk_name_state *ns0 = name_to_ns(name0);
		strength = ns0->requirers ?: 1;
	} else {
		strength = 1;
	}

	if (ns->locked) {
		if (ns->chosen.pkg != NULL) {
			dbg_printf(PKG_VER_FMT " selected already for %s\n",
				   PKG_VER_PRINTF(ns->chosen.pkg), name->name);
		} else {
			dbg_printf("%s selected to not be satisfied\n",
				   name->name);
		}
		if (!apk_dep_is_provided(dep, &ns->chosen))
			ss->score.conflicts -= strength;
		return;
	}
	if (name->providers->num == 0) {
		if (!dep->optional)
			ss->score.conflicts -= strength;
		return;
	}

	if (ss->num_decisions > 0) {
		struct apk_name *name0 = decision_to_name(&ss->decisions[ss->num_decisions]);
		dbg_printf("%s: uninheriting flags and pinning from %s\n",
			   name->name, name0->name);
		uninherit_name_state(ss->db, name, name0);
		/* note: for perfection, we should revert here to the
		 * *previous* value, but that'd require keeping track
		 * of it which would require dynamic memory allocations.
		 * in practice this is good enough. */
		if (ns->last_touched_decision > ss->num_decisions)
			ns->last_touched_decision = ss->num_decisions;
	}

	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		struct apk_package *pkg0 = p0->pkg;
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);

		if (ps0 == NULL || ps0->locked ||
		    ss->topology_position < pkg0->topology_hard)
			continue;

		if (!apk_dep_is_provided(dep, p0)) {
			ps0->conflicts--;
			dbg_printf(PKG_VER_FMT ": conflicts-- -> %d\n",
				   PKG_VER_PRINTF(pkg0),
				   ps0->conflicts);
		}
	}

	if (!dep->optional)
		ns->requirers -= strength;

	demote_name(ss, name);
}

static int reconsider_name(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name_state *ns = name_to_ns(name);
	struct apk_score minscore, score;
	struct apk_provider *next_p = NULL;
	unsigned int next_topology = 0, options = 0;
	int i, score_locked = FALSE;

	subscore(&ss->minimum_penalty, &ns->minimum_penalty);
	ns->minimum_penalty = (struct apk_score) { .score = 0 };

	score = ss->score;
	addscore(&score, &ss->minimum_penalty);

	if (!ns->none_excluded) {
		get_unassigned_score(name, &minscore);
		if (cmpscore2(&score, &minscore, &ss->best_score) >= 0) {
			dbg_printf("%s: pruning none, score too high "SCORE_FMT"+"SCORE_FMT">="SCORE_FMT"\n",
				name->name,
				SCORE_PRINTF(&score),
				SCORE_PRINTF(&minscore),
				SCORE_PRINTF(&ss->best_score));
			return push_decision(ss, name, NULL, DECISION_EXCLUDE, BRANCH_NO, FALSE);
		}
	} else {
		minscore.score = -1;
	}

	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		struct apk_package *pkg0 = p0->pkg;
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);
		struct apk_score pkg0_score;

		if (ps0 == NULL || ps0->locked ||
		    ss->topology_position < pkg0->topology_hard ||
		    ((pkg0->ipkg == NULL && !pkg_available(ss->db, pkg0))))
			continue;

		score_locked = get_topology_score(ss, ns, pkg0, &pkg0_score);

		/* viable alternative? */
		if (cmpscore2(&score, &pkg0_score, &ss->best_score) >= 0)
			return push_decision(ss, name, pkg0, DECISION_EXCLUDE, BRANCH_NO, FALSE);

		/* find the minimum penalty */
		if (cmpscore(&pkg0_score, &minscore) < 0)
			minscore = pkg0_score;

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
		dbg_printf("reconsider_name: %s: no options locking none\n",
			name->name);
		if (ns->none_excluded)
			return SOLVERR_PRUNED;
		return push_decision(ss, name, NULL, DECISION_ASSIGN, BRANCH_NO, FALSE);
	} else if (options == 1 && score_locked && ns->none_excluded) {
		dbg_printf("reconsider_name: %s: only one choice left with known score, locking.\n",
			name->name);
		return push_decision(ss, name, next_p->pkg, DECISION_ASSIGN, BRANCH_NO, FALSE);
	}

	ns->chosen = *next_p;
	ns->minimum_penalty = minscore;
	addscore(&ss->minimum_penalty, &ns->minimum_penalty);

	dbg_printf("reconsider_name: %s: min penalty " SCORE_FMT ", next_pkg=%p\n",
		name->name, SCORE_PRINTF(&minscore), next_p->pkg);

	return SOLVERR_SOLUTION;
}

static int expand_branch(struct apk_solver_state *ss)
{
	struct apk_name *name;
	struct apk_name_state *ns;
	struct apk_package *pkg0 = NULL;
	unsigned int r, topology0 = 0;
	unsigned short allowed_pinning, preferred_pinning;
	unsigned int allowed_repos;
	int primary_decision, branching_point;

	list_for_each_entry(ns, &ss->unsolved_list_head, unsolved_list) {
		name = ns->name;
		if (ns->name_touched) {
			dbg_printf("%s: reconsidering things\n",
				   name->name);
			r = reconsider_name(ss, name);
			if (r != SOLVERR_SOLUTION)
				return r;
			ns->name_touched = 0;
		}
		if (pkg_to_ps(ns->chosen.pkg)->topology_soft < ss->topology_position &&
		    pkg_to_ps(ns->chosen.pkg)->topology_soft > topology0)
			pkg0 = ns->chosen.pkg, topology0 = pkg_to_ps(pkg0)->topology_soft;
		else if (ns->chosen.pkg->topology_hard > topology0)
			pkg0 = ns->chosen.pkg, topology0 = pkg0->topology_hard;
	}
	if (pkg0 == NULL) {
		dbg_printf("expand_branch: solution with score "SCORE_FMT"\n",
			   SCORE_PRINTF(&ss->score));
		return SOLVERR_SOLUTION;
	}

	/* someone needs to provide this name -- find next eligible
	 * provider candidate */
	name = pkg0->name;
	ns = name_to_ns(name);

	if (!ns->none_excluded) {
		struct apk_package_state *ps0 = pkg_to_ps(pkg0);
		if (ps0->conflicts > ns->requirers)
			primary_decision = DECISION_ASSIGN;
		else
			primary_decision = DECISION_EXCLUDE;
		return push_decision(ss, name, NULL, primary_decision, BRANCH_YES, FALSE);
	}

	dbg_printf("expand_branch: "PKG_VER_FMT" score: "SCORE_FMT"\tminpenalty: "SCORE_FMT"\tbest: "SCORE_FMT"\n",
		PKG_VER_PRINTF(pkg0),
		SCORE_PRINTF(&ss->score),
		SCORE_PRINTF(&ss->minimum_penalty),
		SCORE_PRINTF(&ss->best_score));

	preferred_pinning = ns->preferred_pinning ?: APK_DEFAULT_PINNING_MASK;
	allowed_pinning = ns->allowed_pinning | preferred_pinning | APK_DEFAULT_PINNING_MASK;
	allowed_repos = get_pinning_mask_repos(ss->db, allowed_pinning);

	if ((pkg0->repos != 0) && !(pkg0->repos & allowed_repos)) {
		/* pinning has not enabled the package */
		primary_decision = DECISION_EXCLUDE;
		/* but if it is installed, we might consider it */
		if ((pkg0->ipkg == NULL) && (pkg0->filename == NULL))
			branching_point = BRANCH_NO;
		else
			branching_point = BRANCH_YES;
	} else if (ns->requirers == 0 && ns->install_ifs != 0 &&
		   install_if_missing(ss, pkg0)) {
		/* not directly required, and package specific
		 * install_if never triggered */
		primary_decision = DECISION_EXCLUDE;
		branching_point = BRANCH_NO;
	} else if (is_topology_optimum(ss, pkg0)) {
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
	struct apk_name_state *ns;
	int i, n;

	apk_solution_array_resize(&ss->best_solution, ss->assigned_names);

	n = 0;
	for (i = ss->num_decisions; i > 0; i--) {
		struct apk_decision *d = &ss->decisions[i];
		struct apk_package *pkg = decision_to_pkg(d);
		unsigned short pinning;
		unsigned int repos;

		d->found_solution = 1;

		if (pkg == NULL) {
			dbg_printf("record_solution: %s: NOTHING\n",
				   decision_to_name(d)->name);
			continue;
		}

		dbg_printf("record_solution: " PKG_VER_FMT ": %s\n",
			   PKG_VER_PRINTF(pkg),
			   d->type == DECISION_ASSIGN ? "INSTALL" : "no install");

		if (d->type != DECISION_ASSIGN)
			continue;

		ns = name_to_ns(pkg->name);
		pinning = ns->allowed_pinning | ns->preferred_pinning | APK_DEFAULT_PINNING_MASK;
		repos = pkg->repos | (pkg->ipkg ? db->repo_tags[pkg->ipkg->repository_tag].allowed_repos : 0);

		ASSERT(n < ss->assigned_names, "Name assignment overflow\n");
		ss->best_solution->item[n++] = (struct apk_solution_entry){
			.pkg = pkg,
			.reinstall = ns->inherited_reinstall ||
				((ns->solver_flags_local | ss->solver_flags) & APK_SOLVERF_REINSTALL),
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
	struct apk_name_state *ns;
	struct apk_package *pkg;
	struct apk_installed_package *ipkg;
	int i, num_installs = 0, num_removed = 0, ci = 0;

	/* calculate change set size */
	for (i = 0; i < solution->num; i++) {
		pkg = solution->item[i].pkg;
		ns = name_to_ns(pkg->name);
		ns->chosen = APK_PROVIDER_FROM_PACKAGE(pkg);
		ns->in_changeset = 1;
		if ((pkg->ipkg == NULL) ||
		    solution->item[i].reinstall ||
		    solution->item[i].repository_tag != pkg->ipkg->repository_tag)
			num_installs++;
	}
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		name = ipkg->pkg->name;
		ns = name_to_ns(name);
		if ((ns->chosen.pkg == NULL) || !ns->in_changeset)
			num_removed++;
	}

	/* construct changeset */
	apk_change_array_resize(&changeset->changes, num_installs + num_removed);
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		name = ipkg->pkg->name;
		ns = name_to_ns(name);
		if ((ns->chosen.pkg == NULL) || !ns->in_changeset) {
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
	struct apk_name_state *ns = (struct apk_name_state *) name->state_ptr;

	if (ns != NULL) {
#ifdef DEBUG_CHECKS
		ASSERT(ns->requirers == 0, "Requirers is not zero after cleanup\n");
#endif
		free(ns);
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
	struct apk_name_state *ns = name_to_ns_alloc(name);
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
	solver_result_t r = SOLVERR_STOP;
	int i;

	ss = calloc(1, sizeof(struct apk_solver_state));
	ss->db = db;
	ss->solver_flags = solver_flags;
	ss->topology_position = -1;
	ss->best_score = (struct apk_score){ .conflicts = -1 };
	list_init(&ss->unsolved_list_head);

	for (i = 0; i < world->num; i++) {
		sort_name(ss, world->item[i].name);
		name_to_ns(world->item[i].name)->in_world_dependency = 1;
	}
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		sort_name(ss, ipkg->pkg->name);
		name_to_ns(ipkg->pkg->name)->originally_installed = 1;
	}
#if 0
	for (i = 0; i < world->num; i++)
		prepare_name(ss, world->item[i].name);
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list)
		prepare_name(ss, ipkg->pkg->name);
#endif

	ss->max_decisions ++;
	ss->decisions = calloc(1, sizeof(struct apk_decision[ss->max_decisions]));

	foreach_dependency(ss, world, apply_constraint);

	do {
		/* need EXPAND if here, can return SOLUTION|PRUNED|EXPAND */
		r = expand_branch(ss);
		if (r == SOLVERR_SOLUTION) {
			struct apk_score score;

			score = ss->score;
			addscore(&score, &ss->minimum_penalty);

			if (cmpscore(&score, &ss->best_score) < 0) {
				dbg_printf("updating best score "SCORE_FMT" (was: "SCORE_FMT")\n",
					SCORE_PRINTF(&score),
					SCORE_PRINTF(&ss->best_score));

				record_solution(ss);
				ss->best_score = score;
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

	/* collect packages */
	dbg_printf("finished. best score "SCORE_FMT". solution has %d packages.\n",
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
	i = ss->best_score.conflicts;
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
	int i, r = 0, size_diff = 0, size_unit;

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
			if (change->oldpkg != change->newpkg ||
			    (change->reinstall && pkg_available(db, change->newpkg)))
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

		if (apk_dep_is_materialized_or_provided(dep, pkg))
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
