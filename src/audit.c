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

struct audit_ctx {
	unsigned int open_flags;
	int (*audit)(struct apk_database *db);
};

static int audit_file(struct apk_database *db, struct apk_db_file *dbf,
		      const char *name)
{
	struct apk_file_info fi;

	if (apk_file_get_info(db->root_fd, name, APK_FI_NOFOLLOW | dbf->csum.type, &fi) != 0)
		return 1;

	if (dbf->csum.type != APK_CHECKSUM_NONE &&
	    apk_checksum_compare(&fi.csum, &dbf->csum) == 0)
		return 0;

	if (S_ISLNK(fi.mode) && dbf->csum.type == APK_CHECKSUM_NONE)
		return 0;

	return 1;
}

static int audit_directory(apk_hash_item item, void *ctx)
{
	struct apk_database *db = (struct apk_database *) ctx;
	struct apk_db_dir *dbd = (struct apk_db_dir *) item;
	struct apk_db_file *dbf;
	struct apk_file_info fi;
	struct dirent *de;
	apk_blob_t bdir = APK_BLOB_PTR_LEN(dbd->name, dbd->namelen);
	char tmp[PATH_MAX], reason;
	DIR *dir;

	if (!(dbd->flags & APK_DBDIRF_PROTECTED))
		return 0;

	dir = fdopendir(openat(db->root_fd, dbd->name, O_RDONLY | O_CLOEXEC));
	if (dir == NULL)
		return 0;

	while ((de = readdir(dir)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;

		snprintf(tmp, sizeof(tmp), "%s/%s", dbd->name, de->d_name);

		if (apk_file_get_info(db->root_fd, tmp, APK_FI_NOFOLLOW, &fi) < 0)
			continue;

		if ((dbd->flags & APK_DBDIRF_SYMLINKS_ONLY) &&
		    !S_ISLNK(fi.mode))
			continue;

		if (S_ISDIR(fi.mode)) {
			if (apk_db_dir_query(db, APK_BLOB_STR(tmp)) != NULL)
				continue;

			reason = 'D';
		} else {
			dbf = apk_db_file_query(db, bdir, APK_BLOB_STR(de->d_name));
			if (dbf != NULL) {
				if (audit_file(db, dbf, tmp) == 0)
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
	return apk_hash_foreach(&db->installed.dirs, audit_directory, db);
}

static int audit_system(struct apk_database *db)
{
	struct apk_installed_package *ipkg;
	struct apk_package *pkg;
	struct apk_db_dir_instance *diri;
	struct apk_db_file *file;
	struct hlist_node *dn, *fn;
	char name[PATH_MAX];
	int done;

	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		pkg = ipkg->pkg;
		hlist_for_each_entry(diri, dn, &ipkg->owned_dirs, pkg_dirs_list) {
			if (diri->dir->flags & APK_DBDIRF_PROTECTED)
				continue;

			done = 0;
			hlist_for_each_entry(file, fn, &diri->owned_files,
					     diri_files_list) {

				snprintf(name, sizeof(name), "%s/%s",
					 diri->dir->name, file->name);

				if (audit_file(db, file, name) == 0)
					continue;

				if (apk_verbosity < 1) {
					printf("%s\n", pkg->name->name);
					done = 1;
					break;
				}

				printf("M %s\n", name);
			}
			if (done)
				break;
		}
	}

	return 0;
}

static int audit_parse(void *ctx, struct apk_db_options *dbopts,
		       int optch, int optindex, const char *optarg)
{
	struct audit_ctx *actx = (struct audit_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		actx->audit = audit_backup;
		break;
	case 0x10001:
		actx->audit = audit_system;
		break;
	default:
		return -1;
	}
	return 0;
}

static int audit_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct audit_ctx *actx = (struct audit_ctx *) ctx;

	if (actx->audit == NULL)
		return -EINVAL;

	return actx->audit(db);
}

static struct apk_option audit_options[] = {
	{ 0x10000, "backup",
	  "List all modified configuration files that need to be backed up" },
	{ 0x10001, "system", "Verify checksums of all installed files "
		             "(-q to print only modfied packages)" },
};

static struct apk_applet apk_audit = {
	.name = "audit",
	.help = "Audit the filesystem for changes compared to installed "
		"database.",
	.open_flags = APK_OPENF_READ|APK_OPENF_NO_SCRIPTS|APK_OPENF_NO_REPOS,
	.context_size = sizeof(struct audit_ctx),
	.num_options = ARRAY_SIZE(audit_options),
	.options = audit_options,
	.parse = audit_parse,
	.main = audit_main,
};

APK_DEFINE_APPLET(apk_audit);

