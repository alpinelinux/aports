/* solver.c - Alpine Package Keeper (APK)
 * Up- and down-propagating, forwarding checking, deductive dependency solver.
 *
 * Copyright (C) 2008-2013 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include "apk_defines.h"
#include "apk_database.h"
#include "apk_package.h"
#include "apk_solver.h"

#include "apk_print.h"

//#define DEBUG_PRINT

#ifdef DEBUG_PRINT
#include <stdio.h>
#define dbg_printf(args...) fprintf(stderr, args)
#else
#define dbg_printf(args...)
#endif

#define ASSERT(cond, fmt...)	if (!(cond)) { apk_error(fmt); *(char*)NULL = 0; }

struct apk_solver_state {
	struct apk_database *db;
	struct apk_changeset *changeset;
	struct list_head dirty_head;
	struct list_head unresolved_head;
	unsigned int errors;
	unsigned int solver_flags_inherit;
	unsigned int pinning_inherit;
	unsigned int default_repos;
	unsigned ignore_conflict : 1;
};

static struct apk_provider provider_none = {
	.pkg = NULL,
	.version = &apk_null_blob
};

void apk_solver_set_name_flags(struct apk_name *name,
			       unsigned short solver_flags,
			       unsigned short solver_flags_inheritable)
{
	struct apk_provider *p;

	foreach_array_item(p, name->providers) {
		struct apk_package *pkg = p->pkg;
		pkg->ss.solver_flags |= solver_flags;
		pkg->ss.solver_flags_inheritable |= solver_flags_inheritable;
	}
}

static int get_tag(struct apk_database *db, unsigned int pinning_mask, unsigned int repos)
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

static unsigned int get_pkg_repos(struct apk_database *db, struct apk_package *pkg)
{
	return pkg->repos | (pkg->ipkg ? db->repo_tags[pkg->ipkg->repository_tag].allowed_repos : 0);
}

static void mark_error(struct apk_solver_state *ss, struct apk_package *pkg, const char *reason)
{
	if (pkg == NULL || pkg->ss.error)
		return;
	dbg_printf("ERROR PKG: %s: %s\n", pkg->name->name, reason);
	pkg->ss.error = 1;
	ss->errors++;
}

static void queue_dirty(struct apk_solver_state *ss, struct apk_name *name)
{
	if (list_hashed(&name->ss.dirty_list) || name->ss.locked ||
	    (name->ss.requirers == 0 && !name->ss.reevaluate_iif))
		return;

	dbg_printf("queue_dirty: %s\n", name->name);
	list_add_tail(&name->ss.dirty_list, &ss->dirty_head);
}

static void queue_unresolved(struct apk_solver_state *ss, struct apk_name *name)
{
	int want;

	if (name->ss.locked)
		return;

	want = (name->ss.requirers > 0) || (name->ss.has_iif);
	dbg_printf("queue_unresolved: %s, want=%d (requirers=%d, has_iif=%d)\n", name->name, want, name->ss.requirers, name->ss.has_iif);
	if (want && !list_hashed(&name->ss.unresolved_list))
		list_add(&name->ss.unresolved_list, &ss->unresolved_head);
	else if (!want && list_hashed(&name->ss.unresolved_list))
		list_del_init(&name->ss.unresolved_list);
}

static void reevaluate_reverse_deps(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name **pname0, *name0;

	foreach_array_item(pname0, name->rdepends) {
		name0 = *pname0;
		if (!name0->ss.seen)
			continue;
		name0->ss.reevaluate_deps = 1;
		queue_dirty(ss, name0);
	}
}

static void reevaluate_reverse_installif(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name **pname0, *name0;

	foreach_array_item(pname0, name->rinstall_if) {
		name0 = *pname0;
		if (!name0->ss.seen)
			continue;
		if (name0->ss.no_iif)
			continue;
		name0->ss.reevaluate_iif = 1;
		queue_dirty(ss, name0);
	}
}

static void disqualify_package(struct apk_solver_state *ss, struct apk_package *pkg, const char *reason)
{
	struct apk_dependency *p;

	dbg_printf("disqualify_package: " PKG_VER_FMT " (%s)\n", PKG_VER_PRINTF(pkg), reason);
	pkg->ss.pkg_selectable = 0;
	reevaluate_reverse_deps(ss, pkg->name);
	foreach_array_item(p, pkg->provides)
		reevaluate_reverse_deps(ss, p->name);
	reevaluate_reverse_installif(ss, pkg->name);
}

static int dependency_satisfiable(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_provider *p;

	if (dep->conflict && ss->ignore_conflict)
		return TRUE;

	if (name->ss.locked)
		return apk_dep_is_provided(dep, &name->ss.chosen);

	if (name->ss.requirers == 0 && apk_dep_is_provided(dep, &provider_none))
		return TRUE;

	foreach_array_item(p, name->providers)
		if (p->pkg->ss.pkg_selectable && apk_dep_is_provided(dep, p))
			return TRUE;

	return FALSE;
}

static void discover_name(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_database *db = ss->db;
	struct apk_name **pname0;
	struct apk_provider *p;
	struct apk_dependency *dep;
	unsigned int repos;

	if (name->ss.seen)
		return;

	name->ss.seen = 1;
	name->ss.no_iif = 1;
	foreach_array_item(p, name->providers) {
		struct apk_package *pkg = p->pkg;
		if (!pkg->ss.seen) {
			pkg->ss.seen = 1;
			pkg->ss.pinning_allowed = APK_DEFAULT_PINNING_MASK;
			pkg->ss.pinning_preferred = APK_DEFAULT_PINNING_MASK;
			pkg->ss.pkg_available =
				(pkg->filename != NULL) ||
				(pkg->repos & db->available_repos & ~BIT(APK_REPOSITORY_CACHED));
			/* Package is in 'cached' repository if filename is provided,
			 * or it's a 'virtual' package with install_size zero */
			pkg->ss.pkg_selectable =
				(pkg->repos & db->available_repos) ||
				pkg->cached_non_repository ||
				pkg->ipkg;

			/* Prune install_if packages that are no longer available,
			 * currently works only if SOLVERF_AVAILABLE is set in the
			 * global solver flags. */
			pkg->ss.iif_failed =
				(pkg->install_if->num == 0) ||
				((ss->solver_flags_inherit & APK_SOLVERF_AVAILABLE) &&
				 !pkg->ss.pkg_available);

			repos = get_pkg_repos(db, pkg);
			pkg->ss.tag_preferred =
				(pkg->filename != NULL) ||
				(pkg->installed_size == 0) ||
				(repos & ss->default_repos);
			pkg->ss.tag_ok =
				pkg->ss.tag_preferred ||
				pkg->cached_non_repository ||
				pkg->ipkg;

			foreach_array_item(dep, pkg->depends) {
				discover_name(ss, dep->name);
				pkg->ss.max_dep_chain = max(pkg->ss.max_dep_chain,
							    dep->name->ss.max_dep_chain+1);
			}

			dbg_printf("discover " PKG_VER_FMT ": tag_ok=%d, tag_pref=%d max_dep_chain=%d selectable=%d\n",
				PKG_VER_PRINTF(pkg),
				pkg->ss.tag_ok,
				pkg->ss.tag_preferred,
				pkg->ss.max_dep_chain,
				pkg->ss.pkg_selectable);
		}

		name->ss.no_iif &= pkg->ss.iif_failed;
		name->ss.max_dep_chain = max(name->ss.max_dep_chain, pkg->ss.max_dep_chain);

		dbg_printf("discover %s: max_dep_chain=%d no_iif=%d\n",
			name->name, name->ss.max_dep_chain, name->ss.no_iif);
	}
	foreach_array_item(pname0, name->rinstall_if)
		discover_name(ss, *pname0);
}

static void name_requirers_changed(struct apk_solver_state *ss, struct apk_name *name)
{
	queue_unresolved(ss, name);
	reevaluate_reverse_installif(ss, name);
	queue_dirty(ss, name);
}

static void inherit_pinning_and_flags(
	struct apk_solver_state *ss, struct apk_package *pkg, struct apk_package *ppkg)
{
	unsigned int repos = get_pkg_repos(ss->db, pkg);

	if (ppkg != NULL) {
		/* inherited */
		pkg->ss.solver_flags |= ppkg->ss.solver_flags_inheritable;
		pkg->ss.solver_flags_inheritable |= ppkg->ss.solver_flags_inheritable;
		pkg->ss.pinning_allowed |= ppkg->ss.pinning_allowed;
	} else {
		/* world dependency */
		pkg->ss.solver_flags |= ss->solver_flags_inherit;
		pkg->ss.solver_flags_inheritable |= ss->solver_flags_inherit;
		pkg->ss.pinning_allowed |= ss->pinning_inherit;
		/* also prefer main pinnings */
		pkg->ss.pinning_preferred = ss->pinning_inherit;
		pkg->ss.tag_preferred = !!(repos & apk_db_get_pinning_mask_repos(ss->db, pkg->ss.pinning_preferred));
	}
	pkg->ss.tag_ok |= !!(repos & apk_db_get_pinning_mask_repos(ss->db, pkg->ss.pinning_allowed));

	dbg_printf(PKG_VER_FMT ": tag_ok=%d, tag_pref=%d\n",
		PKG_VER_PRINTF(pkg), pkg->ss.tag_ok, pkg->ss.tag_preferred);
}

static void apply_constraint(struct apk_solver_state *ss, struct apk_package *ppkg, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_provider *p0;
	int is_provided;

	dbg_printf("    apply_constraint: %s%s%s" BLOB_FMT "\n",
		dep->conflict ? "!" : "",
		name->name,
		apk_version_op_string(dep->result_mask),
		BLOB_PRINTF(*dep->version));

	if (dep->conflict && ss->ignore_conflict)
		return;

	name->ss.requirers += !dep->conflict;
	if (name->ss.requirers == 1 && !dep->conflict)
		name_requirers_changed(ss, name);

	foreach_array_item(p0, name->providers) {
		struct apk_package *pkg0 = p0->pkg;

		is_provided = apk_dep_is_provided(dep, p0);
		dbg_printf("    apply_constraint: provider: %s-" BLOB_FMT ": %d\n",
			pkg0->name->name, BLOB_PRINTF(*p0->version), is_provided);

		pkg0->ss.conflicts += !is_provided;
		if (unlikely(pkg0->ss.pkg_selectable && pkg0->ss.conflicts))
			disqualify_package(ss, pkg0, "conflicting dependency");

		if (is_provided)
			inherit_pinning_and_flags(ss, pkg0, ppkg);
	}
}

static void exclude_non_providers(struct apk_solver_state *ss, struct apk_name *name, struct apk_name *must_provide, int skip_virtuals)
{
	struct apk_provider *p;
	struct apk_dependency *d;

	if (name == must_provide || ss->ignore_conflict)
		return;

	dbg_printf("%s must provide %s (skip_virtuals=%d)\n", name->name, must_provide->name, skip_virtuals);

	foreach_array_item(p, name->providers) {
		if (p->pkg->name == must_provide || !p->pkg->ss.pkg_selectable ||
		    (skip_virtuals && p->version == &apk_null_blob))
			goto next;
		foreach_array_item(d, p->pkg->provides)
			if (d->name == must_provide || (skip_virtuals && d->version == &apk_null_blob))
				goto next;
		disqualify_package(ss, p->pkg, "provides transitivity");
	next: ;
	}
}

static inline int merge_index(unsigned short *index, int num_options)
{
	if (*index != num_options) return 0;
	*index = num_options + 1;
	return 1;
}

static inline int merge_index_complete(unsigned short *index, int num_options)
{
	int ret;

	ret = (*index == num_options);
	*index = 0;

	return ret;
}

static void reconsider_name(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name *name0, **pname0;
	struct apk_dependency *dep;
	struct apk_package *first_candidate = NULL, *pkg;
	struct apk_provider *p;
	int reevaluate_deps, reevaluate_iif;
	int num_options = 0, num_tag_not_ok = 0, has_iif = 0, no_iif = 1;

	dbg_printf("reconsider_name: %s\n", name->name);

	reevaluate_deps = name->ss.reevaluate_deps;
	reevaluate_iif = name->ss.reevaluate_iif;
	name->ss.reevaluate_deps = 0;
	name->ss.reevaluate_iif = 0;

	/* propagate down by merging common dependencies and
	 * applying new constraints */
	foreach_array_item(p, name->providers) {
		/* check if this pkg's dependencies have become unsatisfiable */
		pkg = p->pkg;
		pkg->ss.dependencies_merged = 0;
		if (reevaluate_deps) {
			if (!pkg->ss.pkg_selectable)
				continue;
			foreach_array_item(dep, pkg->depends) {
				if (!dependency_satisfiable(ss, dep)) {
					disqualify_package(ss, pkg, "dependency no longer satisfiable");
					break;
				}
			}
		}
		if (!pkg->ss.pkg_selectable)
			continue;

		if (reevaluate_iif &&
		    (pkg->ss.iif_triggered == 0 &&
		     pkg->ss.iif_failed == 0)) {
			pkg->ss.iif_triggered = 1;
			pkg->ss.iif_failed = 0;
			foreach_array_item(dep, pkg->install_if) {
				if (!dep->name->ss.locked) {
					pkg->ss.iif_triggered = 0;
					pkg->ss.iif_failed = 0;
					break;
				}
				if (!apk_dep_is_provided(dep, &dep->name->ss.chosen)) {
					pkg->ss.iif_triggered = 0;
					pkg->ss.iif_failed = 1;
					break;
				}
			}
		}
		if (reevaluate_iif && pkg->ss.iif_triggered) {
			foreach_array_item(dep, pkg->install_if)
				inherit_pinning_and_flags(ss, pkg, dep->name->ss.chosen.pkg);
		}
		has_iif |= pkg->ss.iif_triggered;
		no_iif  &= pkg->ss.iif_failed;
		dbg_printf("  "PKG_VER_FMT": iif_triggered=%d iif_failed=%d, no_iif=%d\n",
			PKG_VER_PRINTF(pkg), pkg->ss.iif_triggered, pkg->ss.iif_failed,
			no_iif);

		if (name->ss.requirers == 0)
			continue;

		/* merge common dependencies */
		pkg->ss.dependencies_merged = 1;
		if (first_candidate == NULL)
			first_candidate = pkg;

		/* FIXME: can merge also conflicts */
		foreach_array_item(dep, pkg->depends)
			if (!dep->conflict)
				merge_index(&dep->name->ss.merge_depends, num_options);

		if (merge_index(&pkg->name->ss.merge_provides, num_options))
			pkg->name->ss.has_virtual_provides |= (p->version == &apk_null_blob);
		foreach_array_item(dep, pkg->provides)
			if (merge_index(&dep->name->ss.merge_provides, num_options))
				dep->name->ss.has_virtual_provides |= (dep->version == &apk_null_blob);

		num_tag_not_ok += !pkg->ss.tag_ok;
		num_options++;
	}
	name->ss.has_options = (num_options > 1 || num_tag_not_ok > 0);
	name->ss.has_iif = has_iif;
	name->ss.no_iif = no_iif;
	queue_unresolved(ss, name);

	if (first_candidate != NULL) {
		pkg = first_candidate;
		foreach_array_item(p, name->providers)
			p->pkg->ss.dependencies_used = p->pkg->ss.dependencies_merged;

		/* propagate down common dependencies */
		if (num_options == 1) {
			/* FIXME: keeps increasing counts, use bit fields instead? */
			foreach_array_item(dep, pkg->depends)
				if (merge_index_complete(&dep->name->ss.merge_depends, num_options))
					apply_constraint(ss, pkg, dep);
		} else {
			/* FIXME: could merge versioning bits too */
			foreach_array_item(dep, pkg->depends) {
				name0 = dep->name;
				if (merge_index_complete(&name0->ss.merge_depends, num_options) &&
				    name0->ss.requirers == 0) {
					/* common dependency name with all */
					dbg_printf("%s common dependency: %s\n",
						   name->name, name0->name);
					name0->ss.requirers++;
					name_requirers_changed(ss, name0);
				}
			}
		}

		/* provides transitivity */
		if (merge_index_complete(&pkg->name->ss.merge_provides, num_options))
			exclude_non_providers(ss, pkg->name, name, pkg->name->ss.has_virtual_provides);
		foreach_array_item(dep, pkg->provides)
			if (merge_index_complete(&dep->name->ss.merge_provides, num_options))
				exclude_non_providers(ss, dep->name, name, dep->name->ss.has_virtual_provides);

		pkg->name->ss.has_virtual_provides = 0;
		foreach_array_item(dep, pkg->provides)
			dep->name->ss.has_virtual_provides = 0;
	}

	name->ss.reverse_deps_done = 1;
	foreach_array_item(pname0, name->rdepends) {
		name0 = *pname0;
		if (name0->ss.seen && !name0->ss.locked) {
			name->ss.reverse_deps_done = 0;
			break;
		}
	}

	dbg_printf("reconsider_name: %s [finished], has_options=%d, reverse_deps_done=%d\n",
		name->name, name->ss.has_options, name->ss.reverse_deps_done);
}

static int count_requirers(const struct apk_package *pkg)
{
	int cnt = pkg->name->ss.requirers;
	struct apk_dependency *p;

	foreach_array_item(p, pkg->provides)
		cnt += p->name->ss.requirers;

	return cnt;
}

static int compare_providers(struct apk_solver_state *ss,
			     struct apk_provider *pA, struct apk_provider *pB)
{
	struct apk_database *db = ss->db;
	struct apk_package *pkgA = pA->pkg, *pkgB = pB->pkg;
	unsigned int solver_flags;
	int r;


	/* Prefer existing package */
	if (pkgA == NULL || pkgB == NULL)
		return (pkgA != NULL) - (pkgB != NULL);

	/* Latest version required? */
	solver_flags = pkgA->ss.solver_flags | pkgB->ss.solver_flags;
	if ((solver_flags & APK_SOLVERF_LATEST) &&
	    (pkgA->ss.pinning_allowed == APK_DEFAULT_PINNING_MASK) &&
	    (pkgB->ss.pinning_allowed == APK_DEFAULT_PINNING_MASK)) {
		/* Prefer allowed pinning */
		r = (int)pkgA->ss.tag_ok - (int)pkgB->ss.tag_ok;
		if (r)
			return r;

		/* Prefer available */
		if (solver_flags & APK_SOLVERF_AVAILABLE) {
			r = (int)pkgA->ss.pkg_available - (int)pkgB->ss.pkg_available;
			if (r)
				return r;
		} else if (solver_flags & APK_SOLVERF_REINSTALL) {
			r = (int)pkgA->ss.pkg_selectable - (int)pkgB->ss.pkg_selectable;
			if (r)
				return r;
		}
	} else {
		/* Prefer without errors */
		r = (int)pkgA->ss.pkg_selectable - (int)pkgB->ss.pkg_selectable;
		if (r)
			return r;

		/* Prefer those that were in last dependency merging group */
		r = (int)pkgA->ss.dependencies_used - (int)pkgB->ss.dependencies_used;
		if (r)
			return r;
		r = pkgB->ss.conflicts - pkgA->ss.conflicts;
		if (r)
			return r;

		/* Prefer installed on self-upgrade */
		if (db->performing_self_upgrade && !(solver_flags & APK_SOLVERF_UPGRADE)) {
			r = (pkgA->ipkg != NULL) - (pkgB->ipkg != NULL);
			if (r)
				return r;
		}

		/* Prefer allowed pinning */
		r = (int)pkgA->ss.tag_ok - (int)pkgB->ss.tag_ok;
		if (r)
			return r;

		/* Prefer available */
		if (solver_flags & APK_SOLVERF_AVAILABLE) {
			r = (int)pkgA->ss.pkg_available - (int)pkgB->ss.pkg_available;
			if (r)
				return r;
		}

		/* Prefer preferred pinning */
		r = (int)pkgA->ss.tag_preferred - (int)pkgB->ss.tag_preferred;
		if (r)
			return r;

		/* Prefer highest requirer count. */
		r = count_requirers(pkgA) - count_requirers(pkgB);
		if (r)
			return r;

		/* Prefer installed */
		if (!(solver_flags & APK_SOLVERF_UPGRADE)) {
			r = (pkgA->ipkg != NULL) - (pkgB->ipkg != NULL);
			if (r)
				return r;
		}
	}

	/* Select latest by requested name */
	switch (apk_version_compare_blob(*pA->version, *pB->version)) {
	case APK_VERSION_LESS:
		return -1;
	case APK_VERSION_GREATER:
		return 1;
	}

	/* Select latest by principal name */
	if (pkgA->name == pkgB->name) {
		switch (apk_version_compare_blob(*pkgA->version, *pkgB->version)) {
		case APK_VERSION_LESS:
			return -1;
		case APK_VERSION_GREATER:
			return 1;
		}
	}

	/* Prefer installed (matches here if upgrading) */
	r = (pkgA->ipkg != NULL) - (pkgB->ipkg != NULL);
	if (r)
		return r;

	/* Prefer highest declared provider priority. */
	r = pkgA->provider_priority - pkgB->provider_priority;
	if (r)
		return r;

	/* Prefer without errors (mostly if --latest used, and different provider) */
	r = (int)pkgA->ss.pkg_selectable - (int)pkgB->ss.pkg_selectable;
	if (r)
		return r;

	/* Prefer lowest available repository */
	return ffs(pkgB->repos) - ffs(pkgA->repos);
}

static void assign_name(struct apk_solver_state *ss, struct apk_name *name, struct apk_provider p)
{
	struct apk_provider *p0;

	if (name->ss.locked) {
		/* If both are providing this name without version, it's ok */
		if (p.version == &apk_null_blob &&
		    name->ss.chosen.version == &apk_null_blob)
			return;
		if (ss->ignore_conflict)
			return;
		/* Conflict: providing same name */
		mark_error(ss, p.pkg, "conflict: same name provided");
		mark_error(ss, name->ss.chosen.pkg, "conflict: same name provided");
		return;
	}

	if (p.pkg)
		dbg_printf("assign %s to "PKG_VER_FMT"\n", name->name, PKG_VER_PRINTF(p.pkg));

	name->ss.locked = 1;
	name->ss.chosen = p;
	if (list_hashed(&name->ss.unresolved_list))
		list_del(&name->ss.unresolved_list);
	if (list_hashed(&name->ss.dirty_list))
		list_del(&name->ss.dirty_list);

	/* disqualify all conflicting packages */
	if (!ss->ignore_conflict) {
		foreach_array_item(p0, name->providers) {
			if (p0->pkg == p.pkg)
				continue;
			if (p.version == &apk_null_blob &&
			    p0->version == &apk_null_blob)
				continue;
			disqualify_package(ss, p0->pkg, "conflicting provides");
		}
	}
	reevaluate_reverse_deps(ss, name);
	reevaluate_reverse_installif(ss, name);
}

static void select_package(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_provider chosen = { NULL, &apk_null_blob }, *p;
	struct apk_package *pkg = NULL;
	struct apk_dependency *d;

	dbg_printf("select_package: %s (requirers=%d, iif=%d)\n", name->name, name->ss.requirers, name->ss.has_iif);

	if (name->ss.requirers || name->ss.has_iif) {
		foreach_array_item(p, name->providers) {
			dbg_printf("  consider "PKG_VER_FMT" iif_triggered=%d, tag_ok=%d, selectable=%d, provider_priority=%d, installed=%d\n",
				PKG_VER_PRINTF(p->pkg), p->pkg->ss.iif_triggered, p->pkg->ss.tag_ok, p->pkg->ss.pkg_selectable,
				p->pkg->provider_priority, p->pkg->ipkg != NULL);
			/* Ensure valid pinning and install-if trigger */
			if (name->ss.requirers == 0 &&
			    (!p->pkg->ss.iif_triggered ||
			     !p->pkg->ss.tag_ok))
				continue;
			/* Virtual packages without provider_priority cannot be autoselected,
			 * unless there is only one provider */
			if (p->version == &apk_null_blob &&
			    p->pkg->name->auto_select_virtual == 0 &&
			    p->pkg->name->ss.requirers == 0 &&
			    (p->pkg->provider_priority == 0 && name->providers->num > 1))
				continue;
			if (compare_providers(ss, p, &chosen) > 0)
				chosen = *p;
		}
	}

	pkg = chosen.pkg;
	if (pkg) {
		if (!pkg->ss.pkg_selectable || !pkg->ss.tag_ok) {
			/* Selecting broken or unallowed package */
			mark_error(ss, pkg, "broken package / tag not ok");
		}
		dbg_printf("selecting: " PKG_VER_FMT ", available: %d\n", PKG_VER_PRINTF(pkg), pkg->ss.pkg_selectable);

		assign_name(ss, pkg->name, APK_PROVIDER_FROM_PACKAGE(pkg));
		foreach_array_item(d, pkg->provides)
			assign_name(ss, d->name, APK_PROVIDER_FROM_PROVIDES(pkg, d));

		foreach_array_item(d, pkg->depends)
			apply_constraint(ss, pkg, d);
	} else {
		dbg_printf("selecting: %s [unassigned]\n", name->name);
		assign_name(ss, name, provider_none);
		if (name->ss.requirers > 0) {
			dbg_printf("ERROR NO-PROVIDER: %s\n", name->name);
			ss->errors++;
		}
	}
}

static void record_change(struct apk_solver_state *ss, struct apk_package *opkg, struct apk_package *npkg)
{
	struct apk_changeset *changeset = ss->changeset;
	struct apk_change *change;

	change = apk_change_array_add(&changeset->changes);
	*change = (struct apk_change) {
		.old_pkg = opkg,
		.old_repository_tag = opkg ? opkg->ipkg->repository_tag : 0,
		.new_pkg = npkg,
		.new_repository_tag = npkg ? get_tag(ss->db, npkg->ss.pinning_allowed, get_pkg_repos(ss->db, npkg)) : 0,
		.reinstall = npkg ? !!(npkg->ss.solver_flags & APK_SOLVERF_REINSTALL) : 0,
	};
	if (npkg == NULL)
		changeset->num_remove++;
	else if (opkg == NULL)
		changeset->num_install++;
	else if (npkg != opkg || change->reinstall || change->new_repository_tag != change->old_repository_tag)
		changeset->num_adjust++;
}

static void cset_gen_name_change(struct apk_solver_state *ss, struct apk_name *name);
static void cset_gen_name_remove(struct apk_solver_state *ss, struct apk_package *pkg);
static void cset_gen_dep(struct apk_solver_state *ss, struct apk_package *ppkg, struct apk_dependency *dep);

static void cset_track_deps_added(struct apk_package *pkg)
{
	struct apk_dependency *d;

	foreach_array_item(d, pkg->depends) {
		if (d->conflict || !d->name->ss.installed_name)
			continue;
		d->name->ss.installed_name->ss.requirers++;
	}
}

static void cset_track_deps_removed(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_dependency *d;
	struct apk_package *pkg0;

	foreach_array_item(d, pkg->depends) {
		if (d->conflict || !d->name->ss.installed_name)
			continue;
		if (--d->name->ss.installed_name->ss.requirers > 0)
			continue;
		pkg0 = d->name->ss.installed_pkg;
		if (pkg0 != NULL)
			cset_gen_name_remove(ss, pkg0);
	}
}

static void cset_check_removal_by_deps(struct apk_solver_state *ss, struct apk_package *pkg)
{
	/* NOTE: an orphaned package name may have 0 requirers because it is now being satisfied
	 * through an alternate provider.  In these cases, we will handle this later as an adjustment
	 * operation using cset_gen_name_change().  As such, only insert a removal into the transaction
	 * if there is no other resolved provider.
	 */
	if (pkg->name->ss.requirers == 0 && pkg->name->ss.chosen.pkg == NULL)
		cset_gen_name_remove(ss, pkg);
}

static void cset_check_install_by_iif(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_package *pkg = name->ss.chosen.pkg;
	struct apk_dependency *dep0;

	if (pkg == NULL || !name->ss.seen || name->ss.in_changeset)
		return;

	foreach_array_item(dep0, pkg->install_if) {
		struct apk_name *name0 = dep0->name;
		if (!name0->ss.in_changeset)
			return;
		if (!apk_dep_is_provided(dep0, &name0->ss.chosen))
			return;
	}
	cset_gen_name_change(ss, name);
}

static void cset_check_removal_by_iif(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_package *pkg = name->ss.installed_pkg;
	struct apk_dependency *dep0;

	if (pkg == NULL || name->ss.in_changeset || name->ss.chosen.pkg != NULL)
		return;

	foreach_array_item(dep0, pkg->install_if) {
		if (dep0->name->ss.in_changeset &&
		    dep0->name->ss.chosen.pkg == NULL) {
			cset_check_removal_by_deps(ss, pkg);
			return;
		}
	}
}

static void cset_gen_name_change(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name **pname;
	struct apk_package *pkg, *opkg;
	struct apk_dependency *d;

	if (name->ss.in_changeset) return;

	pkg = name->ss.chosen.pkg;
	if (pkg == NULL || pkg->name != name) {
		/* Original package dependency name was orphaned, emit a removal.
		 * See cset_gen_name_remove() for more details. */
		opkg = name->ss.installed_pkg;
		if (opkg) cset_gen_name_remove(ss, opkg);
		name->ss.in_changeset = 1;

		/* If a replacement is not provided, then we're done here. */
		if (pkg == NULL)
			return;
	}
	if (pkg->ss.in_changeset) return;

	pkg->ss.in_changeset = 1;
	pkg->name->ss.in_changeset = 1;
	foreach_array_item(d, pkg->provides)
		d->name->ss.in_changeset = 1;

	opkg = pkg->name->ss.installed_pkg;
	if (opkg) {
		foreach_array_item(pname, opkg->name->rinstall_if)
			cset_check_removal_by_iif(ss, *pname);
	}

	foreach_array_item(d, pkg->depends)
		cset_gen_dep(ss, pkg, d);

	dbg_printf("Selecting: "PKG_VER_FMT"%s\n", PKG_VER_PRINTF(pkg), pkg->ss.pkg_selectable ? "" : " [NOT SELECTABLE]");
	record_change(ss, opkg, pkg);

	foreach_array_item(pname, pkg->name->rinstall_if)
		cset_check_install_by_iif(ss, *pname);

	cset_track_deps_added(pkg);
	if (opkg)
		cset_track_deps_removed(ss, opkg);
}

static void cset_gen_name_remove0(struct apk_package *pkg0, struct apk_dependency *dep0, struct apk_package *pkg, void *ctx)
{
	cset_gen_name_remove(ctx, pkg0);
}

static void cset_gen_name_remove(struct apk_solver_state *ss, struct apk_package *pkg)
{
	struct apk_name *name = pkg->name, **pname;

	if (pkg->ss.in_changeset ||
	    (name->ss.chosen.pkg != NULL &&
	     name->ss.chosen.pkg->name == name))
		return;

	name->ss.in_changeset = 1;
	pkg->ss.in_changeset = 1;
	apk_pkg_foreach_reverse_dependency(pkg, APK_FOREACH_INSTALLED|APK_DEP_SATISFIES, cset_gen_name_remove0, ss);
	foreach_array_item(pname, pkg->name->rinstall_if)
		cset_check_removal_by_iif(ss, *pname);
	record_change(ss, pkg, NULL);
	cset_track_deps_removed(ss, pkg);
}

static void cset_gen_dep(struct apk_solver_state *ss, struct apk_package *ppkg, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_package *pkg = name->ss.chosen.pkg;

	if (dep->conflict && ss->ignore_conflict)
		return;

	if (!apk_dep_is_provided(dep, &name->ss.chosen))
		mark_error(ss, ppkg, "unfulfilled dependency");

	cset_gen_name_change(ss, name);

	if (pkg && pkg->ss.error)
		mark_error(ss, ppkg, "propagation up");
}

static int cset_reset_name(apk_hash_item item, void *ctx)
{
	struct apk_name *name = (struct apk_name *) item;
	name->ss.installed_pkg = NULL;
	name->ss.installed_name = NULL;
	name->ss.requirers = 0;
	return 0;
}

static void generate_changeset(struct apk_solver_state *ss, struct apk_dependency_array *world)
{
	struct apk_changeset *changeset = ss->changeset;
	struct apk_package *pkg;
	struct apk_installed_package *ipkg;
	struct apk_dependency *d;

	apk_change_array_init(&changeset->changes);

	apk_hash_foreach(&ss->db->available.names, cset_reset_name, NULL);
	list_for_each_entry(ipkg, &ss->db->installed.packages, installed_pkgs_list) {
		pkg = ipkg->pkg;
		pkg->name->ss.installed_pkg = pkg;
		pkg->name->ss.installed_name = pkg->name;
		foreach_array_item(d, pkg->provides)
			if (d->version != &apk_null_blob)
				d->name->ss.installed_name = pkg->name;
	}
	list_for_each_entry(ipkg, &ss->db->installed.packages, installed_pkgs_list)
		cset_track_deps_added(ipkg->pkg);
	list_for_each_entry(ipkg, &ss->db->installed.packages, installed_pkgs_list)
		cset_check_removal_by_deps(ss, ipkg->pkg);

	foreach_array_item(d, world)
		cset_gen_dep(ss, NULL, d);

	/* NOTE: We used to call cset_gen_name_remove() directly here.  While slightly faster, this clobbered
	 * dependency nodes where a new package was provided under a different name (using provides).  As such,
	 * treat everything as a change first and then call cset_gen_name_remove() from there if appropriate.
	 */
	list_for_each_entry(ipkg, &ss->db->installed.packages, installed_pkgs_list)
		cset_gen_name_change(ss, ipkg->pkg->name);

	changeset->num_total_changes =
		changeset->num_install +
		changeset->num_remove +
		changeset->num_adjust;
}

static int free_name(apk_hash_item item, void *ctx)
{
	struct apk_name *name = (struct apk_name *) item;
	memset(&name->ss, 0, sizeof(name->ss));
	return 0;
}

static int free_package(apk_hash_item item, void *ctx)
{
	struct apk_package *pkg = (struct apk_package *) item;
	memset(&pkg->ss, 0, sizeof(pkg->ss));
	return 0;
}

static int cmp_pkgname(const void *p1, const void *p2)
{
	const struct apk_dependency *d1 = p1, *d2 = p2;
	return strcmp(d1->name->name, d2->name->name);
}

static int compare_name_dequeue(const struct apk_name *a, const struct apk_name *b)
{
	int r;

	r = (!!a->ss.requirers) - (!!b->ss.requirers);
	if (r) return -r;

	r = (int)a->priority - (int)b->priority;
	if (r) return r;

	r = a->ss.max_dep_chain - b->ss.max_dep_chain;
	return -r;
}

int apk_solver_solve(struct apk_database *db,
		     unsigned short solver_flags,
		     struct apk_dependency_array *world,
		     struct apk_changeset *changeset)
{
	struct apk_name *name, *name0;
	struct apk_package *pkg;
	struct apk_solver_state ss_data, *ss = &ss_data;
	struct apk_dependency *d;

	qsort(world->item, world->num, sizeof(world->item[0]), cmp_pkgname);

restart:
	memset(ss, 0, sizeof(*ss));
	ss->db = db;
	ss->changeset = changeset;
	ss->default_repos = apk_db_get_pinning_mask_repos(db, APK_DEFAULT_PINNING_MASK);
	ss->ignore_conflict = !!(solver_flags & APK_SOLVERF_IGNORE_CONFLICT);
	list_init(&ss->dirty_head);
	list_init(&ss->unresolved_head);

	dbg_printf("discovering world\n");
	ss->solver_flags_inherit = solver_flags;
	foreach_array_item(d, world) {
		if (!d->broken)
			discover_name(ss, d->name);
	}
	dbg_printf("applying world\n");
	foreach_array_item(d, world) {
		if (!d->broken) {
			ss->pinning_inherit = BIT(d->repository_tag);
			apply_constraint(ss, NULL, d);
		}
	}
	ss->solver_flags_inherit = 0;
	ss->pinning_inherit = 0;
	dbg_printf("applying world [finished]\n");

	do {
		while (!list_empty(&ss->dirty_head)) {
			name = list_pop(&ss->dirty_head, struct apk_name, ss.dirty_list);
			reconsider_name(ss, name);
		}

		name = NULL;
		list_for_each_entry(name0, &ss->unresolved_head, ss.unresolved_list) {
			if (name0->ss.reverse_deps_done && name0->ss.requirers && !name0->ss.has_options) {
				name = name0;
				break;
			}
			if (!name || compare_name_dequeue(name0, name) < 0)
				name = name0;
		}
		if (name == NULL)
			break;

		select_package(ss, name);
	} while (1);

	generate_changeset(ss, world);

	if (ss->errors && (apk_force & APK_FORCE_BROKEN_WORLD)) {
		foreach_array_item(d, world) {
			name = d->name;
			pkg = name->ss.chosen.pkg;
			if (pkg == NULL || pkg->ss.error) {
				d->broken = 1;
				dbg_printf("disabling broken world dep: %s\n", name->name);
			}
		}
		apk_hash_foreach(&db->available.names, free_name, NULL);
		apk_hash_foreach(&db->available.packages, free_package, NULL);
		goto restart;
	}

	apk_hash_foreach(&db->available.names, free_name, NULL);
	apk_hash_foreach(&db->available.packages, free_package, NULL);
	dbg_printf("solver done, errors=%d\n", ss->errors);

	return ss->errors;
}
