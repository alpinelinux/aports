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
#include <malloc.h>

#include "apk_state.h"
#include "apk_database.h"

struct apk_state *apk_state_new(struct apk_database *db)
{
	struct apk_state *state;
	int num_bytes;

	num_bytes = sizeof(struct apk_state) + (db->pkg_id * 2 + 7) / 8;
	state = (struct apk_state*) calloc(1, num_bytes);
	state->refs = 1;
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

static void apk_state_set(struct apk_state *state, int pos, int val)
{
	int byte = pos / 4, offs = pos % 4;

	state->bitarray[byte] &= ~(0x3 << (offs * 2));
	state->bitarray[byte] |= (val & 0x3) << (offs * 2);
}

static int apk_state_get(struct apk_state *state, int pos)
{
	int byte = pos / 4, offs = pos % 4;

	if (state == NULL)
		return APK_STATE_NOT_CONSIDERED;

	return (state->bitarray[byte] >> (offs * 2)) & 0x3;
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
			    name->name, oldpkg->version);
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

int apk_state_commit(struct apk_state *state,
		     struct apk_database *db)
{
	struct progress prog;
	struct apk_change *change;
	int r;

	/* Count what needs to be done */
	memset(&prog, 0, sizeof(prog));
	list_for_each_entry(change, &state->change_list_head, change_list)
		apk_count_change(change, &prog.total);

	/* Go through changes */
	list_for_each_entry(change, &state->change_list_head, change_list) {
		apk_print_change(db, change->oldpkg, change->newpkg);
		prog.pkg = change->newpkg;

		r = apk_db_install_pkg(db, change->oldpkg, change->newpkg,
				       apk_progress ? progress_cb : NULL,
				       &prog);
		if (r != 0)
			return r;

		apk_count_change(change, &prog.done);
	}
	if (apk_progress)
		apk_draw_progress(20, 1);

	return 0;
}

int apk_state_satisfy_name(struct apk_state *state,
			   struct apk_name *name)
{
	struct apk_package *preferred = NULL, *installed = NULL;
	int i, r;

	/* Is something already installed? Or figure out the preferred
	 * package. */
	for (i = 0; i < name->pkgs->num; i++) {
		if (apk_state_get(state, name->pkgs->item[i]->id) ==
		    APK_STATE_INSTALL)
			return 0;

		if (apk_pkg_get_state(name->pkgs->item[i]) == APK_STATE_INSTALL) {
			installed = name->pkgs->item[i];
			if (!apk_upgrade) {
				preferred = installed;
				break;
			}
		}

		if (preferred == NULL) {
			preferred = name->pkgs->item[i];
			continue;
		}

		if (apk_version_compare(APK_BLOB_STR(name->pkgs->item[i]->version),
					APK_BLOB_STR(preferred->version)) ==
		    APK_VERSION_GREATER) {
			preferred = name->pkgs->item[i];
			continue;
		}
	}

	/* FIXME: current code considers only the prefferred package. */

	/* Can we install? */
	switch (apk_state_get(state, preferred->id)) {
	case APK_STATE_INSTALL:
		return 0;
	case APK_STATE_NO_INSTALL:
		return -1;
	}

	/* Update state bits and track changes */
	for (i = 0; i < name->pkgs->num; i++) {
		if (name->pkgs->item[i] != preferred)
			apk_state_set(state, name->pkgs->item[i]->id,
				      APK_STATE_NO_INSTALL);
	}
	apk_state_set(state, preferred->id, APK_STATE_INSTALL);

	r = apk_state_satisfy_deps(state, preferred->depends);
	if (r != 0)
		return r;

	if (preferred != installed) {
		r = apk_state_add_change(state, installed, preferred);
		if (r != 0)
			return r;
	}

	return 0;
}

int apk_state_satisfy_deps(struct apk_state *state,
			   struct apk_dependency_array *deps)
{
	struct apk_name *name;
	int r, i;

	if (deps == NULL)
		return 0;

	for (i = 0; i < deps->num; i++) {
		name = deps->item[i].name;
		if (name->pkgs == NULL) {
			apk_error("No providers for '%s'", name->name);
			return -1;
		}
		r = apk_state_satisfy_name(state, name);
		if (r != 0)
			return r;
	}
	return 0;
}

int apk_state_purge_unneeded(struct apk_state *state,
			     struct apk_database *db)
{
	struct apk_package *pkg;
	int r;

	/* Purge unconsidered packages */
	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list) {
		switch (apk_state_get(state, pkg->id)) {
		case APK_STATE_INSTALL:
		case APK_STATE_NO_INSTALL:
			break;
		default:
			r = apk_state_add_change(state, pkg, NULL);
			if (r != 0)
				return r;
			break;
		}
	}

	return 0;
}

