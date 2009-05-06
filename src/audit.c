/* audit.c - Alpine Package Keeper (APK)
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
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "apk_applet.h"
#include "apk_database.h"

struct audit_ctx {
	int (*action)(struct apk_database *db);
};

static int audit_directory(apk_hash_item item, void *ctx)
{
	struct apk_db_dir *dbd = (struct apk_db_dir *) item;
	struct apk_db_file *dbf;
	struct apk_database *db = (struct apk_database *) ctx;
	struct dirent *de;
	struct stat st;
	struct apk_bstream *bs;
	char tmp[512], reason;
	DIR *dir;
	apk_blob_t bdir = APK_BLOB_STR(dbd->dirname);
	csum_t csum;

	if (!(dbd->flags & APK_DBDIRF_PROTECTED))
		return 0;

	dir = opendir(dbd->dirname);
	if (dir == NULL)
		return 0;

	while ((de = readdir(dir)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;

		snprintf(tmp, sizeof(tmp), "%s/%s",
			 dbd->dirname, de->d_name);

		if (stat(tmp, &st) < 0)
			continue;

		if (S_ISDIR(st.st_mode)) {
			if (apk_db_dir_query(db, APK_BLOB_STR(tmp)) != NULL)
				continue;

			reason = 'D';
		} else {
			dbf = apk_db_file_query(db, bdir, APK_BLOB_STR(de->d_name));
			if (dbf != NULL) {
				bs = apk_bstream_from_file(tmp);
				if (bs == NULL)
					continue;

				bs->close(bs, csum, NULL);

				if (apk_blob_compare(APK_BLOB_BUF(csum),
						     APK_BLOB_BUF(dbf->csum)) == 0)
					continue;

				reason = 'U';
			} else {
				reason = 'A';
			}
		}
		if (apk_verbosity < 1)
			printf("%s\n", tmp);
		else
			printf("%c %s\n", reason, tmp);
	}

	closedir(dir);
	return 0;
}

static int audit_backup(struct apk_database *db)
{
	fchdir(db->root_fd);
	return apk_hash_foreach(&db->installed.dirs, audit_directory, db);
}

static int audit_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct audit_ctx *actx = (struct audit_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		actx->action = audit_backup;
		break;
	default:
		return -1;
	}
	return 0;
}

static int audit_main(void *ctx, int argc, char **argv)
{
	struct audit_ctx *actx = (struct audit_ctx *) ctx;
	struct apk_database db;
	int r;

	if (actx->action == NULL) {
		apk_error("No audit action requested");
		return 2;
	}

	r = apk_db_open(&db, apk_root, APK_OPENF_READ);
	if (r != 0) {
		apk_error("APK database not present");
		return 1;
	}

	r = actx->action(&db);

	apk_db_close(&db);
	return r;
}

static struct option audit_options[] = {
	{ "backup",	no_argument,		NULL, 0x10000 },
};

static struct apk_applet apk_audit = {
	.name = "audit",
	.usage = "--backup",
	.context_size = sizeof(struct audit_ctx),
	.num_options = ARRAY_SIZE(audit_options),
	.options = audit_options,
	.parse = audit_parse,
	.main = audit_main,
};

APK_DEFINE_APPLET(apk_audit);

