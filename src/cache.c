/* cache.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
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
#include "apk_package.h"
#include "apk_print.h"
#include "apk_solver.h"

#define CACHE_CLEAN	BIT(0)
#define CACHE_DOWNLOAD	BIT(1)

static int cache_download(struct apk_database *db)
{
	struct apk_changeset changeset = {};
	struct apk_change *change;
	struct apk_package *pkg;
	struct apk_repository *repo;
	char item[PATH_MAX], cacheitem[PATH_MAX];
	int i, r = 0;

	r = apk_solver_solve(db, 0, db->world, NULL, &changeset);
	if (r != 0) {
		apk_error("Unable to select packages. Run apk fix.");
		return r;
	}

	for (i = 0; i < changeset.changes->num; i++) {
		change = &changeset.changes->item[i];
		pkg = change->newpkg;

		apk_pkg_format_cache(pkg, APK_BLOB_BUF(cacheitem));
		if (faccessat(db->cache_fd, cacheitem, R_OK, 0) == 0)
			continue;

		repo = apk_db_select_repo(db, pkg);
		if (repo == NULL || apk_url_local_file(repo->url) != NULL)
			continue;

		apk_pkg_format_plain(pkg, APK_BLOB_BUF(item));
		r |= apk_cache_download(db, repo->url, pkg->arch,
					item, cacheitem,
					APK_SIGN_VERIFY_IDENTITY);
	}

	return r;
}

static int cache_clean(struct apk_database *db)
{
	char tmp[PATH_MAX];
	DIR *dir;
	struct dirent *de;
	int delete, i;
	apk_blob_t b, bname, bver;
	struct apk_name *name;

	dir = fdopendir(dup(db->cache_fd));
	if (dir == NULL)
		return -1;

	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.')
			continue;

		delete = TRUE;
		do {
			b = APK_BLOB_STR(de->d_name);

			if (apk_blob_compare(b, APK_BLOB_STR("installed")) == 0) {
				delete = FALSE;
				break;
			}

			if (apk_pkg_parse_name(b, &bname, &bver) < 0) {
				/* Index - check for matching repository */
				for (i = 0; i < db->num_repos; i++) {
					apk_cache_format_index(APK_BLOB_BUF(tmp), &db->repos[i]);
					if (apk_blob_compare(b, APK_BLOB_STR(tmp)) != 0)
						continue;
					delete = 0;
					break;
				}
			} else {
				/* Package - search for it */
				name = apk_db_get_name(db, bname);
				if (name == NULL)
					break;
				for (i = 0; i < name->pkgs->num; i++) {
					struct apk_package *pkg = name->pkgs->item[i];

					apk_pkg_format_cache(pkg, APK_BLOB_BUF(tmp));
					if (apk_blob_compare(b, APK_BLOB_STR(tmp)) != 0)
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
				unlinkat(db->cache_fd, de->d_name, 0);
		}
	}

	closedir(dir);
	return 0;
}

static int cache_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	int r = 0, actions = 0;

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

	if (!apk_db_cache_active(db)) {
		apk_error("Package cache is not enabled.\n");
		r = 2;
		goto err;
	}

	if (r == 0 && (actions & CACHE_CLEAN))
		r = cache_clean(db);
	if (r == 0 && (actions & CACHE_DOWNLOAD))
		r = cache_download(db);

err:
	return r;
}

static struct apk_applet apk_cache = {
	.name = "cache",
	.help = "Download missing PACKAGEs to cache directory, or delete "
		"files no longer required. Package caching is enabled by "
		"making /etc/apk/cache a symlink to the directory (on boot "
		"media) that will be used as package cache.",
	.arguments = "sync | clean | download",
	.open_flags = APK_OPENF_READ|APK_OPENF_NO_SCRIPTS|APK_OPENF_NO_INSTALLED|APK_OPENF_CACHE_WRITE,
	.main = cache_main,
};

APK_DEFINE_APPLET(apk_cache);
