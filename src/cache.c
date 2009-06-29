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

#include <dirent.h>
#include <unistd.h>

#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_database.h"

#define CACHE_CLEAN	BIT(0)
#define CACHE_DOWNLOAD	BIT(1)

static int cache_download(struct apk_database *db)
{
	struct apk_package *pkg;
	char pkgfile[256];
	int i, r;

	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list) {
		snprintf(pkgfile, sizeof(pkgfile), "%s-%s.apk",
			 pkg->name->name, pkg->version);
		if (apk_cache_exists(db, pkg->csum, pkgfile))
			continue;
		for (i = 0; i < db->num_repos; i++) {
			if (!(pkg->repos & BIT(i)))
				continue;

			r = apk_cache_download(db, pkg->csum, db->repos[i].url,
					       pkgfile);
			if (r != 0)
				return r;
		}
	}
	return 0;
}

static int cache_clean(struct apk_database *db)
{
	DIR *dir;
	struct dirent *de;
	char path[256];
	int delete, i;
	csum_t csum;

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
			if (strlen(de->d_name) <= sizeof(csum_t)*2+2)
				break;
			if (apk_hexdump_parse(APK_BLOB_BUF(csum),
					      APK_BLOB_PTR_LEN(de->d_name,
					        sizeof(csum_t) * 2)) != 0)
				break;
			if (de->d_name[sizeof(csum_t)*2] != '.')
				break;
			if (strcmp(&de->d_name[sizeof(csum_t)*2+1],
				   apk_index_gz) == 0) {
				/* Index - check for matching repository */
				for (i = 0; i < db->num_repos; i++)
					if (memcmp(db->repos[i].url_csum,
						   csum, sizeof(csum_t)) == 0)
						break;
				delete = (i >= db->num_repos);
			} else {
				/* Package - search for it */
				delete = (apk_db_get_pkg(db, csum) == NULL);
			}
		} while (0);

		if (delete) {
			if (apk_verbosity >= 2)
				apk_message("Deleting %s", de->d_name);
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
		return -100;

	if (strcmp(argv[0], "sync") == 0)
		actions = CACHE_CLEAN | CACHE_DOWNLOAD;
	else if (strcmp(argv[0], "clean") == 0)
		actions = CACHE_CLEAN;
	else if (strcmp(argv[0], "download") == 0)
		actions = CACHE_DOWNLOAD;
	else
		return -100;

	r = apk_db_open(&db, apk_root,
			(actions & CACHE_DOWNLOAD) ? 0 : APK_OPENF_EMPTY_STATE);
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
