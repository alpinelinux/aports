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

static int apk_state_commit_deps(struct apk_state *state,
				 struct apk_database *db,
				 struct apk_dependency_array *deps);

struct apk_state *apk_state_new(struct apk_database *db)
{
	struct apk_state *state;
	int num_bytes;

	num_bytes = sizeof(struct apk_state) + (db->pkg_id * 2 + 7) / 8;
	state = (struct apk_state*) calloc(1, num_bytes);
	state->refs = 1;

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

static int apk_state_commit_pkg(struct apk_state *state,
				struct apk_database *db,
				struct apk_name *name,
				struct apk_package *oldpkg,
				struct apk_package *newpkg)
{
	const char *msg = NULL;
	int r, upgrade = 0;

	if (newpkg != NULL) {
		r = apk_state_commit_deps(state, db, newpkg->depends);
		if (r != 0)
			return r;
	}

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
			upgrade = 1;
			break;
		case APK_VERSION_EQUAL:
			msg = "Re-installing";
			break;
		case APK_VERSION_GREATER:
			msg = "Upgrading";
			upgrade = 1;
			break;
		}
		apk_message("%s %s (%s -> %s)",
			    msg, name->name, oldpkg->version, newpkg->version);
	}

	return apk_db_install_pkg(db, oldpkg, newpkg);
}

static int apk_state_commit_name(struct apk_state *state,
				 struct apk_database *db,
				 struct apk_name *name)
{
	struct apk_package *oldpkg = NULL, *newpkg = NULL;
	int i;

	for (i = 0; i < name->pkgs->num; i++) {
		if (apk_pkg_get_state(name->pkgs->item[i]) == APK_STATE_INSTALL)
			oldpkg = name->pkgs->item[i];
		if (apk_state_get(state, name->pkgs->item[i]->id) == APK_STATE_INSTALL)
			newpkg = name->pkgs->item[i];
	}

	if (oldpkg == NULL && newpkg == NULL)
		return 0;

	/* No reinstallations for now */
	if (newpkg == oldpkg)
		return 0;

	return apk_state_commit_pkg(state, db, name, oldpkg, newpkg);
}

static int apk_state_commit_deps(struct apk_state *state,
				 struct apk_database *db,
				 struct apk_dependency_array *deps)
{
	int r, i;

	if (deps == NULL)
		return 0;

	for (i = 0; i < deps->num; i++) {
		r = apk_state_commit_name(state, db, deps->item[i].name);
		if (r != 0)
			return r;
	}

	return 0;
}

int apk_state_commit(struct apk_state *state,
		     struct apk_database *db)
{
	struct apk_package *pkg, *n;
	int r;

	/* Check all dependencies */
	r = apk_state_commit_deps(state, db, db->world);
	if (r != 0)
		return r;

	/* And purge all installed packages that were not considered */
	list_for_each_entry_safe(pkg, n, &db->installed.packages, installed_pkgs_list)
		apk_state_commit_name(state, db, pkg->name);

	return 0;
}

int apk_state_satisfy_name(struct apk_state *state,
			   struct apk_name *name)
{
	struct apk_package *preferred = NULL;
	int i;
	int upgrading = 1;

	/* Is something already installed? Or figure out the preferred
	 * package. */
	for (i = 0; i < name->pkgs->num; i++) {
		if (apk_state_get(state, name->pkgs->item[i]->id) ==
		    APK_STATE_INSTALL)
			return 0;

		if (preferred == NULL) {
			preferred = name->pkgs->item[i];
			continue;
		}

		if (upgrading) {
			if (apk_version_compare(APK_BLOB_STR(name->pkgs->item[i]->version),
						APK_BLOB_STR(preferred->version)) ==
			    APK_VERSION_GREATER) {
				preferred = name->pkgs->item[i];
				continue;
			}
		} else {
			if (apk_pkg_get_state(name->pkgs->item[i]) ==
			    APK_STATE_INSTALL) {
				preferred = name->pkgs->item[i];
				continue;
			}
		}
	}

	/* Mark conflicting names as no install */
	for (i = 0; i < name->pkgs->num; i++) {
		if (name->pkgs->item[i] != preferred)
			apk_state_set(state, name->pkgs->item[i]->id,
				      APK_STATE_NO_INSTALL);
	}

	return apk_state_pkg_install(state, preferred);
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

void apk_state_pkg_set(struct apk_state *state,
		       struct apk_package *pkg)
{
	apk_state_set(state, pkg->id, APK_STATE_INSTALL);
}

int apk_state_pkg_install(struct apk_state *state,
			  struct apk_package *pkg)
{
	switch (apk_state_get(state, pkg->id)) {
	case APK_STATE_INSTALL:
		return 0;
	case APK_STATE_NO_INSTALL:
		return -1;
	}

	apk_state_set(state, pkg->id, APK_STATE_INSTALL);

	if (pkg->depends == NULL)
		return 0;

	return apk_state_satisfy_deps(state, pkg->depends);
}

int apk_state_pkg_is_installed(struct apk_state *state,
			       struct apk_package *pkg)
{
	return apk_state_get(state, pkg->id);
}

