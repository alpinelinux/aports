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

static int inline ns_locked(apk_name_state_t name)
{
	if (((intptr_t) name) & 0x1)
		return TRUE;
	return FALSE;
}

static int ns_empty(apk_name_state_t name)
{
	return name == NULL;
}

static apk_name_state_t ns_from_pkg(struct apk_package *pkg)
{
	return (apk_name_state_t) (((intptr_t) pkg) | 0x1);
}

static struct apk_package *ns_to_pkg(apk_name_state_t name)
{
	return (struct apk_package *) (((intptr_t) name) & ~0x1);
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

static struct apk_name_choices *name_choices_new(struct apk_name *name)
{
	struct apk_name_choices *nc;

	if (name->pkgs == NULL)
		return NULL;

	nc = malloc(sizeof(struct apk_name_choices) +
		    name->pkgs->num * sizeof(struct apk_package *));
	if (nc == NULL)
		return NULL;

	nc->refs = 1;
	nc->num = name->pkgs->num;
	memcpy(nc->pkgs, name->pkgs->item,
	       name->pkgs->num * sizeof(struct apk_package *));
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

struct apk_state *apk_state_new(struct apk_database *db)
{
	struct apk_state *state;
	int num_bytes;

	num_bytes = sizeof(struct apk_state) + db->name_id * sizeof(char *);
	state = (struct apk_state*) calloc(1, num_bytes);
	state->refs = 1;
	state->num_names = db->name_id;
	list_init(&state->change_list_head);

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
	free(state);
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
	change->oldpkg = oldpkg;
	change->newpkg = newpkg;

	return 0;
}

int apk_state_lock_dependency(struct apk_state *state,
			      struct apk_dependency *dep)
{
	struct apk_name *name = dep->name;
	struct apk_name_choices *c;
        struct apk_package *installed = NULL, *latest = NULL, *use;
	int i;

	if (name->id >= state->num_names)
		return -1;

	if (ns_empty(state->name[name->id])) {
		if (dep->result_mask == APK_DEPMASK_CONFLICT)
			return apk_state_lock_name(state, name, NULL);

		/* This name has not been visited yet.
		 * Construct list of candidates. */
		state->name[name->id] = ns_from_choices(name_choices_new(name));
	}

	if (ns_locked(state->name[name->id])) {
		/* Locked: check that selected package provides
		 * requested version. */
		struct apk_package *pkg = ns_to_pkg(state->name[name->id]);

		/* Locked to not-installed / remove? */
		if (pkg == NULL) {
			if (dep->result_mask == APK_DEPMASK_CONFLICT)
				return 0;
			return -1;
		}

		if (apk_version_compare(APK_BLOB_STR(pkg->version),
					APK_BLOB_STR(dep->version))
		    & dep->result_mask)
			return 0;

		return -1;
	}

	/* Multiple candidates: prune incompatible versions. */
	c = ns_to_choices(state->name[name->id]);
	for (i = 0; i < c->num; i++) {
		if (apk_version_compare(APK_BLOB_STR(c->pkgs[i]->version),
					APK_BLOB_STR(dep->version))
		    & dep->result_mask)
			continue;

		c = name_choices_writable(c);
		c->pkgs[i] = c->pkgs[c->num - 1];
		c->num--;
	}
	if (c->num == 0) {
		name_choices_unref(c);
		return -1;
	}
	if (c->num == 1) {
		struct apk_package *pkg = c->pkgs[0];
		name_choices_unref(c);
		state->name[name->id] = NULL;
		return apk_state_lock_name(state, name, pkg);
	}
	state->name[name->id] = ns_from_choices(c);

#if 1
	/* Get latest and installed packages */
	for (i = 0; i < c->num; i++) {
		struct apk_package *pkg = c->pkgs[i];

		if (apk_pkg_get_state(c->pkgs[i]) == APK_PKG_INSTALLED)
			installed = pkg;

		if (latest == NULL ||
		    apk_version_compare(APK_BLOB_STR(pkg->version),
					APK_BLOB_STR(latest->version)) == APK_VERSION_GREATER)
			latest = pkg;
	}

	/* Choose the best looking candidate.
	 * FIXME: We should instead try all alternatives. */
	if (apk_flags & APK_UPGRADE) {
		use = latest;
	} else {
		if (installed != NULL)
			use = installed;
		else
			use = latest;
	}
	if (use == NULL)
		return -1;

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

static int apk_state_fix_package(struct apk_state *state,
				 struct apk_package *pkg)
{
	int i, r;

	for (i = 0; i < pkg->depends->num; i++) {
		r = apk_state_lock_dependency(state,
					      &pkg->depends->item[i]);
		if (r != 0)
			return -1;
	}
	return 0;
}

static int call_if_dependency_broke(struct apk_state *state,
				    struct apk_package *pkg,
				    struct apk_name *dep_name,
				    int (*cb)(struct apk_state *state,
					      struct apk_package *pkg))
{
	struct apk_package *dep_pkg;
	int k;

	if (pkg->depends == NULL)
		return 0;

	dep_pkg = ns_to_pkg(state->name[dep_name->id]);
	for (k = 0; k < pkg->depends->num; k++) {
		struct apk_dependency *dep = &pkg->depends->item[k];
		if (dep->name != dep_name)
			continue;
		if (dep_pkg == NULL &&
		    dep->result_mask == APK_DEPMASK_CONFLICT)
			continue;
		if (dep_pkg != NULL &&
		    (apk_version_compare(APK_BLOB_STR(dep_pkg->version),
					 APK_BLOB_STR(dep->version))
		     & dep->result_mask))
			continue;
		return cb(state, pkg);
	}

	return 0;
}

static int for_each_broken_reverse_depency(struct apk_state *state,
					   struct apk_name *name,
					   int (*cb)(struct apk_state *state,
						     struct apk_package *pkg))
{
	struct apk_package *pkg0;
	int i, j, r;

	if (name->rdepends == NULL)
		return 0;

	for (i = 0; i < name->rdepends->num; i++) {
		struct apk_name *name0 = name->rdepends->item[i];

		if (ns_locked(state->name[name0->id])) {
			pkg0 = ns_to_pkg(state->name[name0->id]);
			if (pkg0 == NULL)
				continue;
			return call_if_dependency_broke(state, pkg0, name, cb);
		}

		if (!ns_empty(state->name[name0->id])) {
			struct apk_name_choices *ns =
				ns_to_choices(state->name[name0->id]);

			for (j = 0; j < ns->num; j++) {
				if (apk_pkg_get_state(ns->pkgs[j])
				    != APK_PKG_INSTALLED)
					continue;
				r = call_if_dependency_broke(state,
							     ns->pkgs[j],
							     name, cb);
				if (r != 0)
					return r;
			}
			return 0;
		}

		for (j = 0; j < name0->pkgs->num; j++) {
			pkg0 = name0->pkgs->item[j];

			if (apk_pkg_get_state(pkg0) != APK_PKG_INSTALLED)
				continue;

			r = call_if_dependency_broke(state,
						     name0->pkgs->item[j],
						     name, cb);
			if (r != 0)
				return r;
		}
	}

	return 0;
}

static int delete_broken_package(struct apk_state *state,
				 struct apk_package *pkg)
{
	return apk_state_lock_name(state, pkg->name, NULL);
}

static int reinstall_broken_package(struct apk_state *state,
				    struct apk_package *pkg)

{
	struct apk_dependency dep = {
		.name = pkg->name,
		.result_mask = APK_DEPMASK_REQUIRE,
	};
	return apk_state_lock_dependency(state, &dep);
}

int apk_state_lock_name(struct apk_state *state,
			struct apk_name *name,
			struct apk_package *newpkg)
{
	struct apk_package *oldpkg = NULL;
	int i, r;

	if (name->id >= state->num_names)
		return -1;

	if (newpkg == NULL &&
	    (name->flags & APK_NAME_TOPLEVEL) &&
	    !(apk_flags & APK_FORCE)) {
		apk_error("Not deleting top level dependency '%s'. "
			  "Use -f to override.", name->name);
		return -1;
	}

	ns_free(state->name[name->id]);
	state->name[name->id] = ns_from_pkg(newpkg);

	if (name->pkgs != NULL) {
		for (i = 0; i < name->pkgs->num; i++) {
			struct apk_package *pkg = name->pkgs->item[i];

			if (name->pkgs->item[i]->name == name &&
			    apk_pkg_get_state(name->pkgs->item[i])
			    == APK_PKG_INSTALLED)
				oldpkg = pkg;
		}
	}

	/* First we need to make sure the dependants of the old package
	 * still have their dependencies ok. */
	if (oldpkg != NULL) {
		r = for_each_broken_reverse_depency(state, name,
						    newpkg == NULL ?
						    delete_broken_package :
						    reinstall_broken_package);
		if (r != 0)
			return r;
	}

	/* Check that all other dependencies hold for the new package. */
	if (newpkg != NULL && newpkg->depends != NULL) {
		r = apk_state_fix_package(state, newpkg);
		if (r != 0)
			return r;
	}

	/* If the chosen package is installed, all is done here */
	if (oldpkg == newpkg)
		return 0;

	/* Track change */
	r = apk_state_add_change(state, oldpkg, newpkg);
	if (r != 0)
		return r;

	return 0;
}

static void apk_print_change(struct apk_database *db,
			     struct apk_package *oldpkg,
			     struct apk_package *newpkg)
{
	const char *msg = NULL;
	int r;
	struct apk_name *name;

	if (oldpkg != NULL)
		name = oldpkg->name;
	else
		name = newpkg->name;

	if (oldpkg == NULL) {
		apk_message("Installing %s (%s)",
			    name->name, newpkg->version);
	} else if (newpkg == NULL) {
		apk_message("Purging %s (%s)",
			    name->name,
			    oldpkg->version);
	} else {
		r = apk_version_compare(APK_BLOB_STR(newpkg->version),
					APK_BLOB_STR(oldpkg->version));
		switch (r) {
		case APK_VERSION_LESS:
			msg = "Downgrading";
			break;
		case APK_VERSION_EQUAL:
			msg = "Re-installing";
			break;
		case APK_VERSION_GREATER:
			msg = "Upgrading";
			break;
		}
		apk_message("%s %s (%s -> %s)",
			    msg, name->name, oldpkg->version,
			    newpkg->version);
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

static inline void apk_draw_progress(int x, int last)
{
	char tmp[] =
		"-[                    ]- "
		"\b\b\b\b\b\b\b\b\b\b\b\b\b"
		"\b\b\b\b\b\b\b\b\b\b\b\b";
	int i;

	for (i = 0; i < x; i++)
		tmp[2+i] = '#';

	fwrite(tmp, last ? 25 : sizeof(tmp)-1, 1, stderr);
	fflush(stderr);
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

        count = muldiv(20, prog->done.bytes + prog->done.packages + partial,
		       prog->total.bytes + prog->total.packages);

	if (prog->count != count)
		apk_draw_progress(count, 0);
	prog->count = count;
}

static int dump_packages(struct apk_state *state,
			 int (*cmp)(struct apk_change *change),
			 const char *msg)
{
	struct apk_change *change;
	struct apk_name *name;
	int match = 0;

	list_for_each_entry(change, &state->change_list_head, change_list) {
		if (!cmp(change))
			continue;
		if (match == 0)
			fprintf(stderr, "%s:\n ", msg);
		if (change->newpkg != NULL)
			name = change->newpkg->name;
		else
			name = change->oldpkg->name;

		fprintf(stderr, " %s%s", name->name,
			(name->flags & APK_NAME_TOPLEVEL) ? "*" : "");
		match++;
	}
	if (match)
		fprintf(stderr, "\n");
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
	if (apk_version_compare(APK_BLOB_STR(change->newpkg->version),
				APK_BLOB_STR(change->oldpkg->version))
	    & APK_VERSION_LESS)
		return 1;
	return 0;
}

static int cmp_upgrade(struct apk_change *change)
{
	if (change->newpkg == NULL || change->oldpkg == NULL)
		return 0;
	if (apk_version_compare(APK_BLOB_STR(change->newpkg->version),
				APK_BLOB_STR(change->oldpkg->version))
	    & APK_VERSION_GREATER)
		return 1;
	return 0;
}

static int fail_if_something_broke(struct apk_state *state,
				   struct apk_package *pkg)

{
	return 1;
}

static int apk_state_autoclean(struct apk_state *state,
			       struct apk_package *pkg)
{
	apk_name_state_t oldns;
	int i, r;

	if (pkg->depends == NULL)
		return 0;

	for (i = 0; i < pkg->depends->num; i++) {
		struct apk_name *n = pkg->depends->item[i].name;

		if (ns_locked(state->name[n->id]))
			continue;
		if (n->flags & APK_NAME_TOPLEVEL)
			continue;

		oldns = state->name[n->id];
		state->name[n->id] = ns_from_pkg(NULL);
		r = for_each_broken_reverse_depency(state, n,
						    fail_if_something_broke);
		state->name[n->id] = oldns;

		if (r == 0) {
			r = apk_state_lock_name(state, n, NULL);
			if (r != 0)
				return r;
		}
	}
	return 0;
}

int apk_state_commit(struct apk_state *state,
		     struct apk_database *db)
{
	struct progress prog;
	struct apk_change *change;
	int size_diff = 0;
	int r;

	/* Count what needs to be done */
	memset(&prog, 0, sizeof(prog));
	list_for_each_entry(change, &state->change_list_head, change_list) {
		if (change->newpkg == NULL)
			apk_state_autoclean(state, change->oldpkg);
		apk_count_change(change, &prog.total);
		if (change->newpkg)
			size_diff += change->newpkg->installed_size;
		if (change->oldpkg)
			size_diff -= change->oldpkg->installed_size;
	}
	size_diff /= 1024;

	if (apk_verbosity > 1) {
		r = dump_packages(state, cmp_remove,
				  "The following packages will be REMOVED");
		r += dump_packages(state, cmp_downgrade,
				   "The following packages will be DOWNGRADED");
		if (r || apk_verbosity > 2) {
			dump_packages(state, cmp_new,
				      "The following NEW packages will be installed");
			dump_packages(state, cmp_upgrade,
				      "The following packages will be upgraded");
			fprintf(stderr, "%d kB of %s\n", abs(size_diff),
				(size_diff < 0) ?
				"disk space will be freed" :
				"additional disk space will be used");
			fprintf(stderr, "Do you want to continue [Y/n]? ");
			fflush(stderr);
			r = fgetc(stdin);
			if (r != 'y' && r != 'Y')
				return -1;
		}
	}

	/* Go through changes */
	list_for_each_entry(change, &state->change_list_head, change_list) {
		apk_print_change(db, change->oldpkg, change->newpkg);
		prog.pkg = change->newpkg;

		if (!(apk_flags & APK_SIMULATE)) {
			r = apk_db_install_pkg(db,
					       change->oldpkg, change->newpkg,
					       (apk_flags & APK_PROGRESS) ? progress_cb : NULL,
					       &prog);
			if (r != 0)
				return r;
		}

		apk_count_change(change, &prog.done);
	}
	if (apk_flags & APK_PROGRESS)
		apk_draw_progress(20, 1);

	if (!(apk_flags & APK_SIMULATE))
		apk_db_write_config(db);

	apk_message("OK: %d packages, %d dirs, %d files",
		    db->installed.stats.packages,
		    db->installed.stats.dirs,
		    db->installed.stats.files);

	return 0;
}
