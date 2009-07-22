/* cache.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>

#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_state.h"
#include "apk_package.h"

#define CACHE_CLEAN	BIT(0)
#define CACHE_DOWNLOAD	BIT(1)

static int cache_download(struct apk_database *db)
{
	struct apk_state *state;
	struct apk_change *change;
	struct apk_package *pkg;
	char pkgfile[256];
	int i, r;

	if (db->world == NULL)
		return 0;

	state = apk_state_new(db);
	for (i = 0; i < db->world->num; i++) {
		r = apk_state_lock_dependency(state, &db->world->item[i]);
		if (r != 0) {
			apk_error("Unable to select version for '%s'",
				  db->world->item[i].name->name);
			goto err;
		}
	}

	list_for_each_entry(change, &state->change_list_head, change_list) {
		pkg = change->newpkg;
		snprintf(pkgfile, sizeof(pkgfile), "%s-%s.apk",
			 pkg->name->name, pkg->version);
		if (apk_cache_exists(db, &pkg->csum, pkgfile))
			continue;
		for (i = 0; i < db->num_repos; i++) {
			if (!(pkg->repos & BIT(i)))
				continue;

			r = apk_cache_download(db, &pkg->csum, db->repos[i].url,
					       pkgfile, APK_SIGN_VERIFY_IDENTITY);
			if (r != 0)
				return r;
		}
	}

err:
	apk_state_unref(state);
	return r;
}

static int cache_clean(struct apk_database *db)
{
	DIR *dir;
	struct dirent *de;
	char path[256], csum[APK_CACHE_CSUM_BYTES];
	int delete, i;
	apk_blob_t b, bname, bver;
	struct apk_name *name;

	snprintf(path, sizeof(path), "%s/%s", db->root, db->cache_dir);
	if (chdir(path) != 0)
		return -1;

	dir = opendir(path);
	if (dir == NULL)
		return -1;

	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.')
			continue;
		delete = TRUE;
		do {
			b = APK_BLOB_STR(de->d_name);
			apk_blob_pull_hexdump(&b, APK_BLOB_BUF(csum));
			apk_blob_pull_char(&b, '.');

			if (apk_blob_compare(b, APK_BLOB_STR(apk_index_gz)) == 0 ||
			    apk_blob_compare(b, APK_BLOB_STR(apkindex_tar_gz)) == 0) {
				/* Index - check for matching repository */
				for (i = 0; i < db->num_repos; i++) {
					if (memcmp(db->repos[i].csum.data,
						   csum, APK_CACHE_CSUM_BYTES) != 0)
						continue;
					delete = 0;
					break;
				}
			} else if (b.len > 4 &&
				   memcmp(b.ptr+b.len-4, ".apk", 4) == 0) {
				/* Package - search for it */
				if (apk_pkg_parse_name(b, &bname, &bver) < 0)
					break;

				name = apk_db_get_name(db, bname);
				if (name == NULL || name->pkgs == NULL)
					break;

				for (i = 0; i < name->pkgs->num; i++) {
					struct apk_package *pkg = name->pkgs->item[i];
					if (memcmp(pkg->csum.data, csum, APK_CACHE_CSUM_BYTES) != 0)
						continue;
					delete = 0;
					break;
				}
			}
		} while (0);

		if (delete) {
			if (apk_verbosity >= 2)
				apk_message("deleting %s", de->d_name);
			if (!(apk_flags & APK_SIMULATE))
				unlink(de->d_name);
		}
	}

	closedir(dir);
	return 0;
}

static int cache_main(void *ctx, int argc, char **argv)
{
	struct apk_database db;
	int actions = 0;
	int r;

	if (argc != 1)
		return -EINVAL;

	if (strcmp(argv[0], "sync") == 0)
		actions = CACHE_CLEAN | CACHE_DOWNLOAD;
	else if (strcmp(argv[0], "clean") == 0)
		actions = CACHE_CLEAN;
	else if (strcmp(argv[0], "download") == 0)
		actions = CACHE_DOWNLOAD;
	else
		return -EINVAL;

	r = apk_db_open(&db, apk_root,
			APK_OPENF_NO_SCRIPTS | APK_OPENF_NO_INSTALLED);
	if (r != 0)
		return r;

	if (!apk_db_cache_active(&db)) {
		apk_error("Package cache is not enabled.\n");
		r = 2;
		goto err;
	}

	if (r == 0 && (actions & CACHE_CLEAN))
		r = cache_clean(&db);
	if (r == 0 && (actions & CACHE_DOWNLOAD))
		r = cache_download(&db);

err:
	apk_db_close(&db);
	return r;
}

static struct apk_applet apk_cache = {
	.name = "cache",
	.help = "Download missing PACKAGEs to cache directory, or delete "
		"files no longer required. Package caching is enabled by "
		"making /etc/apk/cache a symlink to the directory (on boot "
		"media) that will be used as package cache.",
	.arguments = "sync | clean | download",
	.main = cache_main,
};

APK_DEFINE_APPLET(apk_cache);
