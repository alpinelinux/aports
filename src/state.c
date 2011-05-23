/* state.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>

#include "apk_state.h"
#include "apk_database.h"
#include "apk_print.h"

struct apk_name_choices {
	unsigned short refs, num;
	struct apk_package *pkgs[];
};

#if 0
struct apk_deferred_state {
	unsigned int preference;
	struct apk_state *state;
};
#endif

int apk_state_prune_dependency(struct apk_state *state,
			       struct apk_dependency *dep);

#define APK_NS_LOCKED			0x00000001
#define APK_NS_PENDING			0x00000002
#define APK_NS_ERROR			0x00000004

static void apk_state_record_conflict(struct apk_state *state,
				      struct apk_package *pkg)
{
	struct apk_name *name = pkg->name;

	state->name[name->id] = (void*) (((intptr_t) pkg) | APK_NS_ERROR | APK_NS_LOCKED);
	*apk_package_array_add(&state->conflicts) = pkg;
}

static int inline ns_locked(apk_name_state_t name)
{
	if (((intptr_t) name) & APK_NS_LOCKED)
		return TRUE;
	return FALSE;
}

static int inline ns_pending(apk_name_state_t name)
{
	if (((intptr_t) name) & APK_NS_PENDING)
		return TRUE;
	return FALSE;
}

static int inline ns_error(apk_name_state_t name)
{
	if (((intptr_t) name) & APK_NS_ERROR)
		return TRUE;
	return FALSE;
}

static int ns_empty(apk_name_state_t name)
{
	return name == NULL;
}

static apk_name_state_t ns_from_pkg(struct apk_package *pkg)
{
	return (apk_name_state_t) (((intptr_t) pkg) | APK_NS_LOCKED | APK_NS_PENDING);
}

static apk_name_state_t ns_from_pkg_non_pending(struct apk_package *pkg)
{
	return (apk_name_state_t) (((intptr_t) pkg) | APK_NS_LOCKED);
}

static struct apk_package *ns_to_pkg(apk_name_state_t name)
{
	return (struct apk_package *)
		(((intptr_t) name) & ~(APK_NS_LOCKED | APK_NS_PENDING | APK_NS_ERROR));
}

static apk_name_state_t ns_from_choices(struct apk_name_choices *nc)
{
	if (nc == NULL)
		return ns_from_pkg(NULL);
	return (apk_name_state_t) nc;
}

static struct apk_name_choices *ns_to_choices(apk_name_state_t name)
{
	return (struct apk_name_choices *) name;
}

static struct apk_name_choices *name_choices_new(struct apk_database *db,
						 struct apk_name *name)
{
	struct apk_name_choices *nc;
	int i, j;

	if (name->pkgs->num == 0)
		return NULL;

	nc = malloc(sizeof(struct apk_name_choices) +
		    name->pkgs->num * sizeof(struct apk_package *));
	if (nc == NULL)
		return NULL;

	nc->refs = 1;
	nc->num = name->pkgs->num;
	memcpy(nc->pkgs, name->pkgs->item,
	       name->pkgs->num * sizeof(struct apk_package *));

	for (j = 0; j < nc->num; ) {
		if (nc->pkgs[j]->filename != APK_PKG_UNINSTALLABLE) {
			j++;
		} else {
			nc->pkgs[j] = nc->pkgs[nc->num - 1];
			nc->num--;
		}
	}

	if (name->flags & APK_NAME_TOPLEVEL_OVERRIDE)
		return nc;

	/* Check for global dependencies */
	for (i = 0; i < db->world->num; i++) {
		struct apk_dependency *dep = &db->world->item[i];

		if (dep->name != name)
			continue;

		if (apk_flags & APK_PREFER_AVAILABLE) {
			dep->version = apk_blob_atomize(APK_BLOB_NULL);
			dep->result_mask = APK_DEPMASK_REQUIRE;
		} else {
			for (j = 0; j < nc->num; ) {
				if (apk_dep_is_satisfied(dep, nc->pkgs[j])) {
					j++;
				} else {
					nc->pkgs[j] = nc->pkgs[nc->num - 1];
					nc->num--;
				}
			}
		}
		break;
	}

	return nc;
}

static void name_choices_unref(struct apk_name_choices *nc)
{
	if (--nc->refs == 0)
		free(nc);
}

static struct apk_name_choices *name_choices_writable(struct apk_name_choices *nc)
{
	struct apk_name_choices *n;

	if (nc->refs == 1)
		return nc;

	n = malloc(sizeof(struct apk_name_choices) +
		   nc->num * sizeof(struct apk_package *));
	if (n == NULL)
		return NULL;

	n->refs = 1;
	n->num = nc->num;
	memcpy(n->pkgs, nc->pkgs, nc->num * sizeof(struct apk_package *));
	name_choices_unref(nc);

	return n;
}

static void ns_free(apk_name_state_t name)
{
	if (!ns_empty(name) && !ns_locked(name))
		name_choices_unref(ns_to_choices(name));
}

static inline int apk_state_pkg_available(struct apk_state *state,
					  struct apk_package *pkg)
{
	if (pkg->ipkg != NULL)
		return TRUE;
	if (pkg->installed_size == 0)
		return TRUE;
	if (pkg->filename != NULL)
		return TRUE;
	if (apk_db_select_repo(state->db, pkg) != NULL)
		return TRUE;
	return FALSE;
}

struct apk_state *apk_state_new(struct apk_database *db)
{
	struct apk_state *state;
	int num_bytes;

	num_bytes = sizeof(struct apk_state) + db->name_id * sizeof(char *);
	state = (struct apk_state*) calloc(1, num_bytes);
	state->refs = 1;
	state->num_names = db->name_id;
	state->db = db;
	state->print_ok = 1;
	list_init(&state->change_list_head);

	apk_name_array_init(&state->missing);
	apk_package_array_init(&state->conflicts);

	return state;
}

struct apk_state *apk_state_dup(struct apk_state *state)
{
	state->refs++;
	return state;
}

void apk_state_unref(struct apk_state *state)
{
	if (--state->refs > 0)
		return;

	apk_package_array_free(&state->conflicts);
	apk_name_array_free(&state->missing);
	free(state);
}

static struct apk_package *get_locked_or_installed_package(
					struct apk_state *state,
					struct apk_name *name)
{
	int i;

	if (ns_locked(state->name[name->id]))
		return ns_to_pkg(state->name[name->id]);

	if (!ns_empty(state->name[name->id])) {
		struct apk_name_choices *ns =
			ns_to_choices(state->name[name->id]);

		for (i = 0; i < ns->num; i++) {
			if (ns->pkgs[i]->ipkg != NULL)
				return ns->pkgs[i];
		}
		return NULL;
	}

	for (i = 0; i < name->pkgs->num; i++) {
		if (name->pkgs->item[i]->ipkg != NULL)
			return name->pkgs->item[i];
	}
	return NULL;
}

static int check_dependency(struct apk_state *state,
			    struct apk_dependency *dep)
{
	struct apk_package *pkg;

	pkg = get_locked_or_installed_package(state, dep->name);
	if (pkg == NULL && dep->result_mask != APK_DEPMASK_CONFLICT)
		return 0;
	if (!apk_dep_is_satisfied(dep, pkg))
		return 0;

	return 1;
}

static int check_dependency_array(struct apk_state *state,
				  struct apk_dependency_array *da)
{
	int i;

	for (i = 0; i < da->num; i++) {
		if (!check_dependency(state, &da->item[i]))
			return 0;
	}

	return da->num;
}

static int apk_state_add_change(struct apk_state *state,
				struct apk_package *oldpkg,
				struct apk_package *newpkg)
{
	struct apk_change *change;

	change = (struct apk_change *) malloc(sizeof(struct apk_change));
	if (change == NULL)
		return -1;

	list_init(&change->change_list);
	list_add_tail(&change->change_list, &state->change_list_head);
	state->num_changes++;
	change->oldpkg = oldpkg;
	change->newpkg = newpkg;

	return 0;
}

/* returns:
 *   -1 error
 *    0 locked entry matches and is ok
 *   +n this many candidates on apk_name_choices for the name
 */
int apk_state_prune_dependency(struct apk_state *state,
			       struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_choices *c;
	int i;

	if (name->id >= state->num_names)
		return -1;

	if (ns_empty(state->name[name->id])) {
		if (dep->result_mask == APK_DEPMASK_CONFLICT) {
			state->name[name->id] = ns_from_pkg(NULL);
			return 1;
		}

		/* This name has not been visited yet.
		 * Construct list of candidates. */
		state->name[name->id] = ns_from_choices(name_choices_new(state->db, name));
	}

	if (ns_locked(state->name[name->id])) {
		/* Locked: check that selected package provides
		 * requested version. */
		struct apk_package *pkg = ns_to_pkg(state->name[name->id]);

		/* Locked to not-installed / remove? */
		if (ns_error(state->name[name->id])) {
			return -1;
		} else if (pkg == NULL) {
			if (dep->result_mask != APK_DEPMASK_CONFLICT) {
				if (ns_pending(state->name[name->id])) {
					state->name[name->id] = ns_from_pkg_non_pending(NULL);
					*apk_name_array_add(&state->missing) = name;
				}
				return -1;
			}
		} else {
			if (!apk_dep_is_satisfied(dep, pkg))
				return -1;
		}

		if (ns_pending(state->name[name->id]))
			return 1;

		return 0;
	}

	/* Multiple candidates: prune incompatible versions. */
	c = ns_to_choices(state->name[name->id]);
	i = 0;
	while (i < c->num) {
		if (apk_dep_is_satisfied(dep, c->pkgs[i])) {
			i++;
			continue;
		}

		c = name_choices_writable(c);
		c->pkgs[i] = c->pkgs[c->num - 1];
		c->num--;
	}
	if (c->num == 1 && apk_state_pkg_available(state, c->pkgs[0])) {
		struct apk_package *pkg = c->pkgs[0];
		name_choices_unref(c);
		state->name[name->id] = ns_from_pkg(pkg);
		return 1;
	}
	if (c->num <= 1) {
		name_choices_unref(c);
		state->name[name->id] = ns_from_pkg(NULL);
		*apk_name_array_add(&state->missing) = name;
		return -1;
	}

	state->name[name->id] = ns_from_choices(c);
	return c->num;
}

int apk_state_autolock_name(struct apk_state *state, struct apk_name *name,
			    int install_if)
{
	struct apk_name_choices *c;
	struct apk_package *installed = NULL, *latest = NULL, *use;
	int i;

	if (ns_pending(state->name[name->id]))
		return apk_state_lock_name(state, name, ns_to_pkg(state->name[name->id]));
	if (ns_locked(state->name[name->id]))
		return 0;
	if (ns_empty(state->name[name->id])) {
		/* This name has not been visited yet.
		 * Construct list of candidates. */
		state->name[name->id] = ns_from_choices(name_choices_new(state->db, name));
	}

	c = ns_to_choices(state->name[name->id]);
#if 1
	/* Get latest and installed packages */
	for (i = 0; i < c->num; i++) {
		struct apk_package *pkg = c->pkgs[i];

		if (install_if &&
		    !check_dependency_array(state, pkg->install_if))
			continue;

		if (pkg->ipkg != NULL)
			installed = pkg;
		else if (!apk_state_pkg_available(state, pkg))
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

	/* Choose the best looking candidate.
	 * FIXME: We should instead try all alternatives. */
	if (apk_flags & APK_UPGRADE) {
		use = latest;
	} else {
		if (installed != NULL &&
		    (installed->repos != 0 ||
		     !(name->flags & APK_NAME_REINSTALL)))
			use = installed;
		else
			use = latest;
	}
	if (use == NULL)
		return -2;

	/* Install_if check did not result in package selection change:
	 * do not lock the package yet as the preferency might change
	 * later. */
	if (install_if && use->ipkg != NULL)
		return 0;

	return apk_state_lock_name(state, name, use);
#else
	/* If any of the choices is installed, we are good. Otherwise,
	 * the caller needs to install this dependency. */
	for (i = 0; i < c->num; i++)
		if (apk_pkg_get_state(c->pkgs[i]) == APK_PKG_INSTALLED)
			return 0;

	/* Queue for deferred solution. */
	return 0;
#endif
}

int apk_state_lock_dependency(struct apk_state *state,
			      struct apk_dependency *dep)
{
	int r;

	r = apk_state_prune_dependency(state, dep);
	if (r <= 0)
		return r;

	return apk_state_autolock_name(state, dep->name, FALSE);
}

static int apk_state_fix_package(struct apk_state *state,
				 struct apk_package *pkg)
{
	int i, r, ret = 0;

	if (pkg == NULL)
		return 0;

	for (i = 0; i < pkg->depends->num; i++) {
		if ((pkg->depends->item[i].name->flags & APK_NAME_TOPLEVEL_OVERRIDE) &&
		    check_dependency(state, &pkg->depends->item[i])) {
			r = apk_state_prune_dependency(state,
						       &pkg->depends->item[i]);
			if (r < 0)
				ret = -1;
		} else {
			r = apk_state_lock_dependency(state,
						      &pkg->depends->item[i]);
			if (r != 0)
				ret = -1;
		}
	}
	return ret;
}

static int call_if_dependency_broke(struct apk_state *state,
				    struct apk_package *pkg,
				    struct apk_name *dep_name,
				    int (*cb)(struct apk_state *state,
					      struct apk_package *pkg,
					      struct apk_dependency *dep,
					      void *ctx),
				    void *ctx)
{
	struct apk_package *dep_pkg;
	int k;

	dep_pkg = ns_to_pkg(state->name[dep_name->id]);
	for (k = 0; k < pkg->depends->num; k++) {
		struct apk_dependency *dep = &pkg->depends->item[k];
		if (dep->name != dep_name)
			continue;
		if (dep_pkg == NULL &&
		    dep->result_mask == APK_DEPMASK_CONFLICT)
			continue;
		if (dep_pkg != NULL &&
		    apk_dep_is_satisfied(dep, dep_pkg))
			continue;
		return cb(state, pkg, dep, ctx);
	}

	return 0;
}

static int for_each_broken_reverse_depency(struct apk_state *state,
					   struct apk_name *name,
					   int (*cb)(struct apk_state *state,
						     struct apk_package *pkg,
						     struct apk_dependency *dep,
						     void *ctx),
					   void *ctx)
{
	struct apk_package *pkg0;
	int i, r;

	for (i = 0; i < name->rdepends->num; i++) {
		struct apk_name *name0 = name->rdepends->item[i];

		pkg0 = get_locked_or_installed_package(state, name0);
		if (pkg0 == NULL)
			continue;

		r = call_if_dependency_broke(state, pkg0, name,
					     cb, ctx);
		if (r != 0)
			return r;
	}

	return 0;
}

static int delete_broken_package(struct apk_state *state,
				 struct apk_package *pkg,
				 struct apk_dependency *dep,
				 void *ctx)
{
	return apk_state_lock_name(state, pkg->name, NULL);
}

static int reinstall_broken_package(struct apk_state *state,
				    struct apk_package *pkg,
				    struct apk_dependency *dep,
				    void *ctx)

{
	struct apk_dependency dep0 = {
		.name = pkg->name,
		.version = apk_blob_atomize(APK_BLOB_NULL),
		.result_mask = APK_DEPMASK_REQUIRE,
	};
	return apk_state_lock_dependency(state, &dep0);
}

int apk_state_lock_name(struct apk_state *state,
			struct apk_name *name,
			struct apk_package *newpkg)
{
	struct apk_package *oldpkg = NULL;
	int i, r;

	if (name->id >= state->num_names)
		return -1;

	ns_free(state->name[name->id]);
	state->name[name->id] = ns_from_pkg_non_pending(newpkg);

	for (i = 0; i < name->pkgs->num; i++) {
		struct apk_package *pkg = name->pkgs->item[i];
		if (name->pkgs->item[i]->name == name &&
		    pkg->ipkg != NULL)
			oldpkg = pkg;
	}

	/* First we need to make sure the dependants of the old package
	 * still have their dependencies ok. */
	if (oldpkg != NULL) {
		r = for_each_broken_reverse_depency(state, name,
						    newpkg == NULL ?
						    delete_broken_package :
						    reinstall_broken_package,
						    NULL);
		if (r != 0) {
			apk_state_record_conflict(state, newpkg);
			return r;
		}
	}

	/* Check that all other dependencies hold for the new package. */
	r = apk_state_fix_package(state, newpkg);
	if (r != 0) {
		apk_state_record_conflict(state, newpkg);
		return r;
	}

	/* If the chosen package is installed, all is done here */
	if ((oldpkg != newpkg) ||
	    (newpkg != NULL && (newpkg->name->flags & APK_NAME_REINSTALL))) {
		/* Track change */
		r = apk_state_add_change(state, oldpkg, newpkg);
		if (r != 0)
			return r;
	}

	/* Check all reverse install_if's */
	if (newpkg != NULL) {
		for (i = 0; i < newpkg->name->rinstall_if->num; i++)
			apk_state_autolock_name(state, newpkg->name->rinstall_if->item[i], TRUE);
	}

	return 0;
}

static void apk_print_change(struct apk_database *db,
			     struct apk_package *oldpkg,
			     struct apk_package *newpkg,
			     int num, int total)
{
	const char *msg = NULL;
	int r;
	struct apk_name *name;
	char status[64];

	snprintf(status, sizeof(status), "(%i/%i)", num, total);
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

static void apk_count_change(struct apk_change *change, struct apk_stats *stats)
{
	if (change->newpkg != NULL) {
		stats->bytes += change->newpkg->installed_size;
		stats->packages ++;
	}
	if (change->oldpkg != NULL)
		stats->packages ++;
}

static void apk_draw_progress(int percent)
{
	const int bar_width = (apk_screen_width - 7);
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
		apk_draw_progress(count);
	prog->count = count;
}

static int dump_packages(struct apk_state *state,
			 int (*cmp)(struct apk_change *change),
			 const char *msg)
{
	struct apk_change *change;
	struct apk_name *name;
	struct apk_indent indent = { 0, 2 };
	char tmp[256];
	int match = 0, i;

	list_for_each_entry(change, &state->change_list_head, change_list) {
		if (!cmp(change))
			continue;
		if (match == 0)
			printf("%s:\n ", msg);
		if (change->newpkg != NULL)
			name = change->newpkg->name;
		else
			name = change->oldpkg->name;

		i = snprintf(tmp, sizeof(tmp), "%s%s", name->name,
			     (name->flags & APK_NAME_TOPLEVEL) ? "*" : "");
		apk_print_indented(&indent, APK_BLOB_PTR_LEN(tmp, i));
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

static int fail_if_something_broke(struct apk_state *state,
				   struct apk_package *pkg,
				   struct apk_dependency *dep,
				   void *ctx)

{
	return 1;
}

static int apk_state_autoclean(struct apk_state *state,
			       struct apk_package *pkg)
{
	apk_name_state_t oldns;
	int i, r;

	for (i = 0; i < pkg->depends->num; i++) {
		struct apk_name *n = pkg->depends->item[i].name;

		if (ns_locked(state->name[n->id]))
			continue;
		if (n->flags & APK_NAME_TOPLEVEL)
			continue;

		oldns = state->name[n->id];
		state->name[n->id] = ns_from_pkg(NULL);
		r = for_each_broken_reverse_depency(state, n,
						    fail_if_something_broke,
						    NULL);
		state->name[n->id] = oldns;

		if (r == 0) {
			r = apk_state_lock_name(state, n, NULL);
			if (r != 0)
				return r;
		}
	}

	for (i = 0; i < pkg->name->rinstall_if->num; i++) {
		struct apk_name *n = pkg->name->rinstall_if->item[i];

		if (ns_locked(state->name[n->id]))
			continue;
		if (n->flags & APK_NAME_TOPLEVEL)
			continue;

		r = apk_state_autolock_name(state, n, TRUE);
		if (r == -2) {
			r = apk_state_lock_name(state, n, NULL);
			if (r != 0)
				return r;
		}
	}

	return 0;
}

struct error_state {
	struct apk_indent indent;
	struct apk_package *prevpkg;
};

static int print_dep(struct apk_state *state,
		     struct apk_package *pkg,
		     struct apk_dependency *dep,
		     void *ctx)
{
	struct error_state *es = (struct error_state *) ctx;
	apk_blob_t blob;
	char buf[256];
	int len;

	if (pkg != es->prevpkg) {
		printf("\n");
		es->indent.x = 0;
		len = snprintf(buf, sizeof(buf), PKG_VER_FMT ":",
			       PKG_VER_PRINTF(pkg));
		apk_print_indented(&es->indent, APK_BLOB_PTR_LEN(buf, len));
		es->prevpkg = pkg;
	}

	blob = APK_BLOB_BUF(buf);
	apk_blob_push_dep(&blob, dep);
	blob = apk_blob_pushed(APK_BLOB_BUF(buf), blob);
	apk_print_indented(&es->indent, blob);

	return 0;
}

void apk_state_print_errors(struct apk_state *state)
{
	struct apk_package *pkg;
	struct error_state es;
	int i, j, r;

	for (i = 0; i < state->conflicts->num; i++) {
		if (i == 0)
			apk_error("Unable to satisfy all dependencies:");

		es.prevpkg = pkg = state->conflicts->item[i];
		es.indent.x =
		printf("  " PKG_VER_FMT ":", PKG_VER_PRINTF(pkg));
		es.indent.indent = es.indent.x + 1;
		for (j = 0; j < pkg->depends->num; j++) {
			r = apk_state_lock_dependency(state,
						      &pkg->depends->item[j]);
			if (r != 0)
				print_dep(state, pkg, &pkg->depends->item[j], &es);
		}

		/* Print conflicting reverse deps */
		for_each_broken_reverse_depency(state, pkg->name,
						print_dep, &es);
		printf("\n");
	}

	for (i = 0; i < state->missing->num; i++) {
		struct apk_name *name = state->missing->item[i];
		if (i == 0) {
			apk_error("Missing packages:");
			es.indent.x = 0;
			es.indent.indent = 2;
		}
		apk_print_indented(&es.indent, APK_BLOB_STR(name->name));
	}
	if (i != 0)
		printf("\n");
}

int apk_state_commit(struct apk_state *state)
{
	struct progress prog;
	struct apk_change *change;
	struct apk_database *db = state->db;
	int n = 0, r = 0, size_diff = 0, toplevel = FALSE, deleteonly = TRUE;

	/* Count what needs to be done */
	memset(&prog, 0, sizeof(prog));
	list_for_each_entry(change, &state->change_list_head, change_list) {
		if (change->newpkg == NULL) {
			if (change->oldpkg->name->flags & APK_NAME_TOPLEVEL)
				toplevel = TRUE;
		} else
			deleteonly = FALSE;
		if (change->oldpkg != NULL)
			apk_state_autoclean(state, change->oldpkg);
		apk_count_change(change, &prog.total);
		if (change->newpkg)
			size_diff += change->newpkg->installed_size;
		if (change->oldpkg)
			size_diff -= change->oldpkg->installed_size;
	}
	size_diff /= 1024;

	if (toplevel &&
	    (apk_flags & (APK_INTERACTIVE | APK_RECURSIVE_DELETE)) == 0) {
		if (!deleteonly)
			return -1;

		dump_packages(state, cmp_remove,
			      "The top-level dependencies have been updated "
			      "but the following packages are not removed");
		goto update_state;
	}

	if (apk_verbosity > 1 || (apk_flags & APK_INTERACTIVE)) {
		r = dump_packages(state, cmp_remove,
				  "The following packages will be REMOVED");
		r += dump_packages(state, cmp_downgrade,
				   "The following packages will be DOWNGRADED");
		if (r || (apk_flags & APK_INTERACTIVE) || apk_verbosity > 2) {
			dump_packages(state, cmp_new,
				      "The following NEW packages will be installed");
			dump_packages(state, cmp_upgrade,
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
	n = 0;
	list_for_each_entry(change, &state->change_list_head, change_list) {
		n++;
		apk_print_change(db, change->oldpkg, change->newpkg, n, state->num_changes);
		if (apk_flags & APK_PROGRESS)
			apk_draw_progress(prog.count);
		prog.pkg = change->newpkg;

		if (!(apk_flags & APK_SIMULATE)) {
			r = apk_db_install_pkg(db,
					       change->oldpkg, change->newpkg,
					       (apk_flags & APK_PROGRESS) ? progress_cb : NULL,
					       &prog);
			if (r != 0)
				break;

			if (change->oldpkg != NULL &&
			    change->newpkg == NULL &&
			    change->oldpkg->name->flags & APK_NAME_TOPLEVEL) {
				change->oldpkg->name->flags &= ~APK_NAME_TOPLEVEL;
				apk_deps_del(&db->world, change->oldpkg->name);
			}
		}

		apk_count_change(change, &prog.done);
	}
	if (apk_flags & APK_PROGRESS)
		apk_draw_progress(100);

update_state:
	apk_db_run_triggers(db);
	apk_db_write_config(db);

	if (r == 0 && state->print_ok)
		apk_message("OK: %d packages, %d dirs, %d files",
			    db->installed.stats.packages,
			    db->installed.stats.dirs,
			    db->installed.stats.files);

	return r;
}
