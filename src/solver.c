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
	unsigned int solver_flags;
	unsigned int errors;
	unsigned int num_selections, num_solution_entries;
};

static struct apk_provider provider_none = {
	.pkg = NULL,
	.version = &apk_null_blob
};

void apk_solver_set_name_flags(struct apk_name *name,
			       unsigned short solver_flags,
			       unsigned short solver_flags_inheritable)
{
}

static void foreach_dependency(struct apk_solver_state *ss, struct apk_dependency_array *deps,
			       void (*func)(struct apk_solver_state *ss, struct apk_dependency *dep))
{
	int i;
	for (i = 0; i < deps->num; i++)
		func(ss, &deps->item[i]);
}

static void foreach_name(struct apk_solver_state *ss, struct apk_name_array *names,
			 void (*func)(struct apk_solver_state *ss, struct apk_name *name))
{
	int i;
	for (i = 0; i < names->num; i++)
		if (names->item[i]->ss.seen)
			func(ss, names->item[i]);
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
	dbg_printf("queue_unresolved: %s, want=%d\n", name->name, want);
	if (want && !list_hashed(&name->ss.unresolved_list))
		list_add(&name->ss.unresolved_list, &ss->unresolved_head);
	else if (!want && list_hashed(&name->ss.unresolved_list))
		list_del_init(&name->ss.unresolved_list);
}

static void reevaluate_deps(struct apk_solver_state *ss, struct apk_name *name)
{
	name->ss.reevaluate_deps = 1;
	queue_dirty(ss, name);
}

static void reevaluate_installif(struct apk_solver_state *ss, struct apk_name *name)
{
	name->ss.reevaluate_iif = 1;
	queue_dirty(ss, name);
}

static void disqualify_package(struct apk_solver_state *ss, struct apk_package *pkg, const char *reason)
{
	int i;

	dbg_printf("disqualify_package: " PKG_VER_FMT " (%s)\n", PKG_VER_PRINTF(pkg), reason);
	pkg->ss.available = 0;

	foreach_name(ss, pkg->name->rdepends, reevaluate_deps);
	for (i = 0; i < pkg->provides->num; i++)
		foreach_name(ss, pkg->provides->item[i].name->rdepends, reevaluate_deps);
	foreach_name(ss, pkg->name->rinstall_if, reevaluate_installif);
}

static int dependency_satisfiable(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	int i;

	if (name->ss.locked)
		return apk_dep_is_provided(dep, &name->ss.chosen);

	if (name->ss.requirers == 0 && apk_dep_is_provided(dep, &provider_none))
		return TRUE;

	for (i = 0; i < name->providers->num; i++) {
		struct apk_package *pkg = name->providers->item[i].pkg;
		if (!pkg->ss.available)
			continue;
		if (apk_dep_is_provided(dep, &name->providers->item[i]))
			return TRUE;
	}

	return FALSE;
}

static void discover_name(struct apk_solver_state *ss, struct apk_name *name);
static void discover_names(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	discover_name(ss, dep->name);
}

static void discover_name(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_database *db = ss->db;
	int i, j;
	unsigned long mask = apk_db_get_pinning_mask_repos(ss->db, APK_DEFAULT_PINNING_MASK);

	if (name->ss.seen)
		return;
	name->ss.seen = 1;
	for (i = 0; i < name->providers->num; i++) {
		struct apk_package *pkg = name->providers->item[i].pkg;
		if (pkg->ss.seen)
			continue;
		pkg->ss.seen = 1;
		if ((pkg->ipkg != NULL) ||
		    (pkg->repos & db->available_repos & mask))
			pkg->ss.available = 1;
		foreach_dependency(ss, pkg->depends, discover_names);
		for (j = 0; j < pkg->depends->num; j++)
			pkg->ss.max_dep_chain = max(pkg->ss.max_dep_chain, name->ss.max_dep_chain+1);
		name->ss.max_dep_chain = max(name->ss.max_dep_chain, pkg->ss.max_dep_chain);
	}
	/* Can't use foreach_name, as it checks the seen flag */
	for (i = 0; i < name->rinstall_if->num; i++)
		discover_name(ss, name->rinstall_if->item[i]);
}

static void name_requirers_changed(struct apk_solver_state *ss, struct apk_name *name)
{
	queue_unresolved(ss, name);
	foreach_name(ss, name->rinstall_if, reevaluate_installif);
	queue_dirty(ss, name);
}

static void apply_constraint(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	int i;

	dbg_printf("apply_constraint: %s%s%s" BLOB_FMT "\n",
		dep->conflict ? "!" : "",
		name->name,
		apk_version_op_string(dep->result_mask),
		BLOB_PRINTF(*dep->version));

	name->ss.requirers += !dep->conflict;
	name_requirers_changed(ss, name);

	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		struct apk_package *pkg0 = p0->pkg;
		int is_provided;

		is_provided = apk_dep_is_provided(dep, p0);
		dbg_printf("apply_constraint: provider: %s-" BLOB_FMT ": %d\n",
			pkg0->name->name, BLOB_PRINTF(*p0->version), is_provided);

		pkg0->ss.conflicts += !is_provided;
		if (unlikely(pkg0->ss.available && pkg0->ss.conflicts))
			disqualify_package(ss, pkg0, "conflicting dependency");
	}
}

static void reconsider_name(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_name *name0;
	struct apk_dependency *dep;
	struct apk_package *pkg;
	int i, j, reevaluate_deps, reevaluate_iif;
	int first_candidate = -1, last_candidate = 0;
	int num_options = 0, has_iif = 0;

	dbg_printf("reconsider_name: %s\n", name->name);

	reevaluate_deps = name->ss.reevaluate_deps;
	name->ss.reevaluate_deps = 0;

	reevaluate_iif = name->ss.reevaluate_iif;
	name->ss.reevaluate_iif = 0;

	/* propagate down by merging common dependencies and
	 * applying new constraints */
	for (i = 0; i < name->providers->num; i++) {
		struct apk_provider *p0 = &name->providers->item[i];
		struct apk_package *pkg = p0->pkg;

		/* check if this pkg's dependencies have become unsatisfiable */
		if (reevaluate_deps) {
			if (!pkg->ss.available)
				continue;
			for (j = 0; j < pkg->depends->num; j++) {
				dep = &pkg->depends->item[j];
				if (!dependency_satisfiable(ss, dep)) {
					disqualify_package(ss, pkg, "dependency no longer satisfiable");
					break;
				}
			}
		}
		if (!pkg->ss.available)
			continue;

		if (reevaluate_iif) {
			for (j = 0; j < pkg->install_if->num; j++) {
				dep = &pkg->install_if->item[j];
				if (!dependency_satisfiable(ss, dep))
					break;
			}
			pkg->ss.iif_triggered = (j >= pkg->install_if->num);
			has_iif |= pkg->ss.iif_triggered;
		}

		if (name->ss.requirers == 0 && !pkg->ss.iif_triggered)
			continue;

		num_options++;

		/* merge common dependencies */
		if (first_candidate == -1)
			first_candidate = i;
		for (j = 0; j < pkg->depends->num; j++) {
			dep = &pkg->depends->item[j];
			if (dep->conflict /*&& dep->result_mask != APK_DEPMASK_ANY*/)
				continue;
			name0 = dep->name;
			if (name0->ss.merge_index == last_candidate)
				name0->ss.merge_index = i + 1;
		}
		last_candidate = i + 1;
	}
	name->ss.has_options = (num_options > 1);
	name->ss.has_iif = has_iif;
	queue_unresolved(ss, name);

	if (first_candidate != -1) {
		/* TODO: could merge versioning bits too */
		/* propagate down common dependencies */
		pkg = name->providers->item[first_candidate].pkg;
		for (j = 0; j < pkg->depends->num; j++) {
			dep = &pkg->depends->item[j];
			if (dep->conflict && dep->result_mask != APK_DEPMASK_ANY)
				continue;
			name0 = dep->name;
			if (name0->ss.merge_index == last_candidate) {
				/* common dependency name with all */
				if (name0->ss.requirers == 0) {
					dbg_printf("%s common dependency: %s\n",
						   name->name, name0->name);
					name0->ss.requirers++;
					name_requirers_changed(ss, name0);
				}
			}
			name0->ss.merge_index = 0;
		}
	}
	dbg_printf("reconsider_name: %s [finished]\n", name->name);
}

static int compare_providers(struct apk_solver_state *ss,
			     struct apk_provider *pA, struct apk_provider *pB)
{
	struct apk_database *db = ss->db;
	struct apk_package *pkgA = pA->pkg, *pkgB = pB->pkg;
	int r;

	/* Prefer existing package */
	if (pkgA == NULL || pkgB == NULL)
		return (pkgA != NULL) - (pkgB != NULL);

	/* Prefer without errors */
	r = (int)pkgA->ss.available - (int)pkgB->ss.available;
	if (r)
		return r;

	/* Prefer available */
	if (ss->solver_flags & APK_SOLVERF_AVAILABLE) {
		r = !!(pkgA->repos & db->available_repos) -
		    !!(pkgB->repos & db->available_repos);
		if (r)
			return r;
	}

	/* Prefer installed */
	/* FIXME: check-per-name flags here too */
	if (!(ss->solver_flags & APK_SOLVERF_UPGRADE)) {
		r = (pkgA->ipkg != NULL) - (pkgB->ipkg != NULL);
		if (r)
			return r;
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

	/* Prefer lowest available repository */
	return ffsl(pkgB->repos) - ffsl(pkgA->repos);
}

static inline void assign_name(struct apk_solver_state *ss, struct apk_name *name, struct apk_provider p)
{
	int i;

	if (name->ss.locked) {
		/* If both are providing this name without version, it's ok */
		if (p.version == &apk_null_blob &&
		    name->ss.chosen.version == &apk_null_blob)
			return;
		/* Othewise providing locked item is an error */
		ss->errors++;
		return;
	}

	dbg_printf("assign %s to "PKG_VER_FMT"\n", name->name, PKG_VER_PRINTF(p.pkg));

	name->ss.locked = 1;
	name->ss.chosen = p;
	if (list_hashed(&name->ss.unresolved_list))
		list_del(&name->ss.unresolved_list);
	if (list_hashed(&name->ss.dirty_list))
		list_del(&name->ss.dirty_list);

	/* disqualify all conflicting packages */
	for (i = 0; i < name->providers->num; i++) {
		if (name->providers->item[i].pkg == p.pkg)
			continue;
		if (p.version == &apk_null_blob &&
		    name->providers->item[i].version == &apk_null_blob)
			continue;
		disqualify_package(ss, name->providers->item[i].pkg,
				   "conflicting provides");
	}

	foreach_name(ss, name->rdepends, reevaluate_deps);
}

static void select_package(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_provider chosen = { NULL, &apk_null_blob };
	struct apk_package *pkg = NULL;
	int i;

	dbg_printf("select_package: %s\n", name->name);

	if (name->ss.requirers || name->ss.has_iif) {
		for (i = 0; i < name->providers->num; i++) {
			if (name->ss.requirers == 0 && !name->providers->item[i].pkg->ss.iif_triggered)
				continue;
			if (compare_providers(ss, &name->providers->item[i], &chosen) > 0)
				chosen = name->providers->item[i];
		}
	}

	pkg = chosen.pkg;
	if (pkg) {
		if (chosen.version == &apk_null_blob) {
			/* Pure virtual package */
			assign_name(ss, name, provider_none);
			ss->errors++;
			return;
		}
		if (!pkg->ss.available)
			ss->errors++;
		dbg_printf("selecting: " PKG_VER_FMT ", available: %d\n", PKG_VER_PRINTF(pkg), pkg->ss.available);
		assign_name(ss, pkg->name, APK_PROVIDER_FROM_PACKAGE(pkg));
		for (i = 0; i < pkg->provides->num; i++) {
			struct apk_dependency *p = &pkg->provides->item[i];
			assign_name(ss, p->name, APK_PROVIDER_FROM_PROVIDES(pkg, p));
		}
		foreach_dependency(ss, pkg->depends, apply_constraint);
		ss->num_selections++;
	} else {
		dbg_printf("selecting: %s [unassigned]\n", name->name);
		assign_name(ss, name, provider_none);
		if (name->ss.requirers)
			ss->errors++;
	}
}

static void generate_change_dep(struct apk_solver_state *ss, struct apk_dependency *dep);
static void generate_change_iif(struct apk_solver_state *ss, struct apk_name *name);

static void generate_change(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_package *pkg = name->ss.chosen.pkg, *opkg;
	struct apk_changeset *changeset = ss->changeset;
	struct apk_change *change;

	if (pkg == NULL)
		return;

	if (pkg->ss.in_changeset)
		return;
	pkg->ss.in_changeset = 1;
	pkg->name->ss.in_changeset = 1;

	foreach_dependency(ss, pkg->depends, generate_change_dep);

	change = &changeset->changes->item[ss->num_solution_entries++];
	dbg_printf("Selecting: "PKG_VER_FMT"%s\n", PKG_VER_PRINTF(pkg), pkg->ss.available ? "" : " [NOT AVAILABLE]");
	opkg = apk_pkg_get_installed(pkg->name);
	*change = (struct apk_change) {
		.old_pkg = opkg,
		.old_repository_tag = opkg ? opkg->ipkg->repository_tag : 0,
		.new_pkg = pkg,
		.new_repository_tag = pkg->ipkg ? pkg->ipkg->repository_tag : 0,
#if 0
		/* FIXME: setup reinstall and repository_tag for solution */
		.reinstall = ps->inherited_reinstall ||
			((name->ss.solver_flags_local | ss->solver_flags) & APK_SOLVERF_REINSTALL),
		.repository_tag = get_tag(db, pinning, repos),
#endif
	};
	if (change->new_pkg == NULL)
		changeset->num_remove++;
	else if (change->old_pkg == NULL)
		changeset->num_install++;
	else if (change->new_pkg != change->old_pkg || change->reinstall ||
		 change->new_repository_tag != change->old_repository_tag)
		changeset->num_adjust++;

	foreach_name(ss, pkg->name->rinstall_if, generate_change_iif);
}

static void generate_change_iif(struct apk_solver_state *ss, struct apk_name *name)
{
	struct apk_package *pkg = name->ss.chosen.pkg;
	int i;

	if (pkg == NULL)
		return;

	for (i = 0; i < pkg->install_if->num; i++) {
		struct apk_dependency *dep0 = &pkg->install_if->item[i];
		struct apk_name *name0 = dep0->name;

		if (!name0->ss.in_changeset)
			return;
		if (!apk_dep_is_provided(dep0, &name0->ss.chosen))
			return;
	}

	generate_change(ss, name);
}

static void generate_change_dep(struct apk_solver_state *ss, struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;

	if (!apk_dep_is_provided(dep, &name->ss.chosen))
		ss->errors++;

	generate_change(ss, name);
}

static void generate_changeset(struct apk_solver_state *ss, struct apk_dependency_array *world)
{
	struct apk_changeset *changeset = ss->changeset;
	struct apk_installed_package *ipkg;

	list_for_each_entry(ipkg, &ss->db->installed.packages, installed_pkgs_list) {
		struct apk_name *name = ipkg->pkg->name;
		if (name->ss.chosen.pkg == NULL && !name->ss.locked)
			ss->num_selections++;
	}

	apk_change_array_resize(&ss->changeset->changes, ss->num_selections);
	foreach_dependency(ss, world, generate_change_dep);

	/* FIXME: could order better the removals of unneeded packages */
	list_for_each_entry(ipkg, &ss->db->installed.packages, installed_pkgs_list) {
		struct apk_name *name = ipkg->pkg->name;
		if (name->ss.chosen.pkg == NULL && !name->ss.in_changeset) {
			struct apk_change *change = &changeset->changes->item[ss->num_solution_entries++];
			*change = (struct apk_change) {
				.old_pkg = ipkg->pkg,
				.new_pkg = NULL,
			};
			changeset->num_remove++;
		}
	}

	changeset->num_total_changes = changeset->num_install + changeset->num_remove + changeset->num_adjust;

#if 1
	/* FIXME: calculate num_solution_entries correctly */
	ASSERT(ss->num_solution_entries <= changeset->changes->num,
		"Got %d changes, but expected %d\n",
		ss->num_solution_entries, changeset->changes->num);
	apk_change_array_resize(&ss->changeset->changes, ss->num_solution_entries);
#else
	ASSERT(ss->num_solution_entries == changeset->changes->num,
		"Got %d changes, but expected %d\n",
		ss->num_solution_entries, changeset->changes->num);
#endif
}

static int free_state(apk_hash_item item, void *ctx)
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

int apk_solver_solve(struct apk_database *db,
		     unsigned short solver_flags,
		     struct apk_dependency_array *world,
		     struct apk_changeset *changeset)
{
	struct apk_name *name, *name0;
	struct apk_solver_state ss_data, *ss = &ss_data;
	int i;

	memset(ss, 0, sizeof(*ss));
	ss->db = db;
	ss->changeset = changeset;
	ss->solver_flags = solver_flags;
	list_init(&ss->dirty_head);
	list_init(&ss->unresolved_head);

	foreach_dependency(ss, world, discover_names);

	/* FIXME: If filename specified, force to use it */

	dbg_printf("applying world\n");
	for (i = 0; i < world->num; i++) {
		struct apk_dependency *dep = &world->item[i];
		name = dep->name;
		name->ss.in_world_dependency = 1;
		name->ss.preferred_pinning = BIT(dep->repository_tag);
		apply_constraint(ss, dep);
	}
	dbg_printf("applying world [finished]\n");

	do {
		while (!list_empty(&ss->dirty_head)) {
			name = list_pop(&ss->dirty_head, struct apk_name, ss.dirty_list);
			reconsider_name(ss, name);
		}

		name = NULL;
		list_for_each_entry(name0, &ss->unresolved_head, ss.unresolved_list) {
			if (!name0->ss.has_options && name0->ss.requirers > 0) {
				name = name0;
				break;
			}
			if (name == NULL)
				goto prefer;
			if (name->ss.requirers > 0 && name0->ss.requirers == 0)
				continue;
			if (name->ss.requirers == 0 && name0->ss.requirers > 0)
				goto prefer;
			if (name0->ss.max_dep_chain > name->ss.max_dep_chain)
				goto prefer;
			continue;
		prefer:
			name = name0;
		}
		if (name == NULL)
			break;

		select_package(ss, name);
	} while (1);

	generate_changeset(ss, world);

	apk_hash_foreach(&db->available.names, free_state, NULL);
	apk_hash_foreach(&db->available.packages, free_package, NULL);

	dbg_printf("solver done, errors=%d\n", ss->errors);

	return ss->errors;
}
