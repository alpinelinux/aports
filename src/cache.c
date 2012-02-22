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
	int i, r, ret = 0;

	r = apk_solver_solve(db, 0, db->world, NULL, &changeset);
	if (r < 0) {
		apk_error("Unable to select packages. Run apk fix.");
		return r;
	}

	for (i = 0; i < changeset.changes->num; i++) {
		change = &changeset.changes->item[i];
		pkg = change->newpkg;

		if (pkg->in_cache)
			continue;

		repo = apk_db_select_repo(db, pkg);
		if (repo == NULL || !apk_repo_is_remote(repo))
			continue;

		apk_pkg_format_cache(pkg, APK_BLOB_BUF(cacheitem));
		apk_pkg_format_plain(pkg, APK_BLOB_BUF(item));
		r = apk_cache_download(db, repo->url, pkg->arch,
					item, cacheitem,
					APK_SIGN_VERIFY_IDENTITY);
		if (r) {
			apk_error("%s: %s", item, apk_error_str(r));
			ret++;
		}
	}

	return ret;
}

static void cache_clean_item(struct apk_database *db, int dirfd, const char *name, struct apk_package *pkg)
{
	char tmp[PATH_MAX];
	apk_blob_t b;
	int i;

	if (pkg != NULL || strcmp(name, "installed") == 0)
		return;

	b = APK_BLOB_STR(name);
	for (i = 0; i < db->num_repos; i++) {
		/* Check if this is a valid index */
		apk_cache_format_index(APK_BLOB_BUF(tmp), &db->repos[i]);
		if (apk_blob_compare(b, APK_BLOB_STR(tmp)) == 0)
			return;
	}

	if (apk_verbosity >= 2)
		apk_message("deleting %s", name);
	if (!(apk_flags & APK_SIMULATE))
		unlinkat(dirfd, name, 0);
}

static int cache_clean(struct apk_database *db)
{
	return apk_db_cache_foreach_item(db, cache_clean_item);
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
