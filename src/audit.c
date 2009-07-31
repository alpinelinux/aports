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
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "apk_applet.h"
#include "apk_database.h"

#define AUDIT_BACKUP BIT(0)
#define AUDIT_SYSTEM BIT(1)

struct audit_ctx {
	int type;
	struct apk_database *db;
};

static int audit_directory(apk_hash_item item, void *ctx)
{
	struct apk_db_dir *dbd = (struct apk_db_dir *) item;
	struct apk_db_file *dbf;
	struct audit_ctx *actx = (struct audit_ctx *) ctx;
	struct apk_database *db = actx->db;
	struct dirent *de;
	struct apk_file_info fi;
	apk_blob_t bdir = APK_BLOB_PTR_LEN(dbd->name, dbd->namelen);
	char tmp[512], reason;
	DIR *dir;

	if (!(actx->type & AUDIT_SYSTEM) && !(dbd->flags & APK_DBDIRF_PROTECTED))
		return 0;
	if (!(actx->type & AUDIT_BACKUP) && (dbd->flags & APK_DBDIRF_PROTECTED))
		return 0;

	dir = fdopendir(openat(db->root_fd, dbd->name, O_RDONLY));
	if (dir == NULL)
		return 0;

	while ((de = readdir(dir)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;

		snprintf(tmp, sizeof(tmp), "%s/%s", dbd->name, de->d_name);

		if (apk_file_get_info(db->root_fd, tmp, APK_CHECKSUM_NONE, &fi) < 0)
			continue;

		if (!(actx->type & AUDIT_SYSTEM) &&
		    (dbd->flags & APK_DBDIRF_SYMLINKS_ONLY) &&
		    !S_ISLNK(fi.mode))
			continue;

		if (S_ISDIR(fi.mode)) {
			if (apk_db_dir_query(db, APK_BLOB_STR(tmp)) != NULL)
				continue;

			reason = 'D';
		} else {
			dbf = apk_db_file_query(db, bdir, APK_BLOB_STR(de->d_name));
			if (dbf != NULL) {
				if (dbf->csum.type != APK_CHECKSUM_NONE &&
				    apk_file_get_info(db->root_fd, tmp, dbf->csum.type, &fi) == 0 &&
				    apk_checksum_compare(&fi.csum, &dbf->csum) == 0)
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

static int audit_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct audit_ctx *actx = (struct audit_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		actx->type |= AUDIT_BACKUP;
		break;
	case 0x10001:
		actx->type |= AUDIT_SYSTEM;
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

	if (actx->type == 0)
		return -EINVAL;

	r = apk_db_open(&db, apk_root, APK_OPENF_READ);
	if (r != 0) {
		apk_error("APK database not present");
		return 1;
	}

	actx->db = &db;

	fchdir(db.root_fd);
	r = apk_hash_foreach(&db.installed.dirs, audit_directory, actx);

	apk_db_close(&db);
	return r;
}

static struct apk_option audit_options[] = {
	{ 0x10000, "backup",
	  "List all modified configuration files that need to be backed up" },
	{ 0x10001, "system", "List all modified non-protected files" },
};

static struct apk_applet apk_audit = {
	.name = "audit",
	.help = "Audit the filesystem for changes compared to installed "
		"database.",
	.context_size = sizeof(struct audit_ctx),
	.num_options = ARRAY_SIZE(audit_options),
	.options = audit_options,
	.parse = audit_parse,
	.main = audit_main,
};

APK_DEFINE_APPLET(apk_audit);

