/* audit.c - Alpine Package Keeper (APK)
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
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_print.h"

enum {
	MODE_BACKUP = 0,
	MODE_SYSTEM
};

struct audit_ctx {
	unsigned mode : 1;
	unsigned recursive : 1;
	unsigned check_permissions : 1;
	unsigned packages_only : 1;
};

static int audit_parse(void *ctx, struct apk_db_options *dbopts,
		       int optch, int optindex, const char *optarg)
{
	struct audit_ctx *actx = (struct audit_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		actx->mode = MODE_BACKUP;
		break;
	case 0x10001:
		actx->mode = MODE_SYSTEM;
		break;
	case 0x10002:
		actx->check_permissions = 1;
		break;
	case 0x10003:
		actx->packages_only = 1;
		break;
	case 'r':
		actx->recursive = 1;
		break;
	default:
		return -1;
	}
	return 0;
}

struct audit_tree_ctx {
	struct audit_ctx *actx;
	struct apk_database *db;
	struct apk_db_dir *dir;
	size_t pathlen;
	char path[PATH_MAX];
};

static int audit_file(struct audit_ctx *actx,
		      struct apk_database *db,
		      struct apk_db_file *dbf,
		      int dirfd, const char *name)
{
	struct apk_file_info fi;

	if (dbf == NULL)
		return 'A';

	if (apk_file_get_info(dirfd, name, APK_FI_NOFOLLOW | dbf->csum.type, &fi) != 0)
		return -EPERM;

	if (dbf->csum.type != APK_CHECKSUM_NONE &&
	    apk_checksum_compare(&fi.csum, &dbf->csum) != 0)
		return 'U';

	if (S_ISLNK(fi.mode) && dbf->csum.type == APK_CHECKSUM_NONE)
		return 'U';

	if (actx->check_permissions &&
	    (dbf->mode != 0 || dbf->uid != 0 || dbf->gid != 0)) {
		if ((fi.mode & 07777) != (dbf->mode & 07777))
			return 'M';
		if (fi.uid != dbf->uid || fi.gid != dbf->gid)
			return 'M';
	}

	return 0;
}

static int audit_directory(struct audit_ctx *actx,
			   struct apk_database *db,
			   struct apk_db_dir *dbd,
			   struct apk_file_info *fi)
{
	if (dbd == NULL || dbd->refs == 1)
		return actx->recursive ? 'd' : 'D';

	if (actx->check_permissions &&
	    (dbd->mode != 0 || dbd->uid != 0 || dbd->gid != 0)) {
		if ((fi->mode & 07777) != (dbd->mode & 07777))
			return 'm';
		if (fi->uid != dbd->uid || fi->gid != dbd->gid)
			return 'm';
	}

	return 0;
}

static void report_audit(struct audit_ctx *actx,
			 char reason, apk_blob_t bfull, struct apk_package *pkg)
{
	if (!reason)
		return;

	if (actx->packages_only) {
		if (pkg == NULL || pkg->state_int != 0)
			return;
		pkg->state_int = 1;
		if (apk_verbosity < 1)
			printf("%s\n", pkg->name->name);
		else
			printf(PKG_VER_FMT "\n", PKG_VER_PRINTF(pkg));
	} else if (apk_verbosity < 1) {
		printf(BLOB_FMT "\n", BLOB_PRINTF(bfull));
	} else
		printf("%c " BLOB_FMT "\n", reason, BLOB_PRINTF(bfull));
}

static int audit_directory_tree_item(void *ctx, int dirfd, const char *name)
{
	struct audit_tree_ctx *atctx = (struct audit_tree_ctx *) ctx;
	apk_blob_t bdir = APK_BLOB_PTR_LEN(atctx->path, atctx->pathlen);
	apk_blob_t bent = APK_BLOB_STR(name);
	apk_blob_t bfull;
	struct audit_ctx *actx = atctx->actx;
	struct apk_database *db = atctx->db;
	struct apk_db_dir *dir = atctx->dir, *child = NULL;
	struct apk_file_info fi;
	int reason = 0;

	if (bdir.len + bent.len + 1 >= sizeof(atctx->path))
		return -ENOMEM;

	if (apk_file_get_info(dirfd, name, APK_FI_NOFOLLOW, &fi) < 0)
		return -EPERM;

	memcpy(&atctx->path[atctx->pathlen], bent.ptr, bent.len);
	atctx->pathlen += bent.len;
	bfull = APK_BLOB_PTR_LEN(atctx->path, atctx->pathlen);

	if (S_ISDIR(fi.mode)) {
		int recurse = TRUE;

		if (actx->mode == MODE_BACKUP) {
			child = apk_db_dir_get(db, bfull);
			if (!child->has_protected_children)
				recurse = FALSE;
			if (!child->protected)
				goto recurse_check;
		} else {
			child = apk_db_dir_query(db, bfull);
			if (child == NULL)
				goto done;
			child = apk_db_dir_ref(child);
		}

		reason = audit_directory(actx, db, child, &fi);
		if (reason < 0)
			goto done;

recurse_check:
		atctx->path[atctx->pathlen++] = '/';
		bfull.len++;
		report_audit(actx, reason, bfull, NULL);
		if (reason != 'D' && recurse) {
			atctx->dir = child;
			reason = apk_dir_foreach_file(
				openat(dirfd, name, O_RDONLY|O_CLOEXEC),
				audit_directory_tree_item, atctx);
			atctx->dir = dir;
		}
		bfull.len--;
		atctx->pathlen--;
	} else {
		struct apk_db_file *dbf;
		struct apk_protected_path_array *ppaths = dir->protected_paths;
		int i, protected = dir->protected, symlinks_only = dir->symlinks_only;

		/* inherit file's protection mask */
		for (i = 0; i < ppaths->num; i++) {
			struct apk_protected_path *ppath = &ppaths->item[i];
			char *slash = strchr(ppath->relative_pattern, '/');
			if (slash == NULL) {
				if (fnmatch(ppath->relative_pattern, name, FNM_PATHNAME) != 0)
					continue;
				protected = ppath->protected;
				symlinks_only = ppath->symlinks_only;
			}
		}

		if (actx->mode == MODE_BACKUP) {
			if (!protected)
				goto done;
			if (symlinks_only && !S_ISLNK(fi.mode))
				goto done;
		} else {
			if (protected)
				goto done;
		}

		dbf = apk_db_file_query(db, bdir, bent);
		reason = audit_file(actx, db, dbf, dirfd, name);
		if (reason < 0)
			goto done;
		if (reason == 'A' && actx->mode == MODE_SYSTEM)
			goto done;
		report_audit(actx, reason, bfull, dbf ? dbf->diri->pkg : NULL);
	}

done:
	if (child)
		apk_db_dir_unref(db, child, FALSE);

	atctx->pathlen -= bent.len;
	return reason < 0 ? reason : 0;
}

static int audit_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct audit_tree_ctx atctx;
	int i, r = 0;

	atctx.db = db;
	atctx.actx = (struct audit_ctx *) ctx;
	atctx.pathlen = 0;
	atctx.path[0] = 0;

	if (argc == 0) {
		atctx.dir = apk_db_dir_get(db, APK_BLOB_PTR_LEN(atctx.path, atctx.pathlen));
		r = apk_dir_foreach_file(dup(db->root_fd), audit_directory_tree_item, &atctx);
		apk_db_dir_unref(db, atctx.dir, FALSE);
	} else {
		for (i = 0; i < argc; i++) {
			if (argv[i][0] != '/') {
				apk_warning("%s: relative path skipped.\n",
					    argv[i]);
				continue;
			}
			argv[i]++;
			atctx.pathlen = strlen(argv[i]);
			memcpy(atctx.path, argv[i], atctx.pathlen);
			if (atctx.path[atctx.pathlen-1] != '/')
				atctx.path[atctx.pathlen++] = '/';

			atctx.dir = apk_db_dir_get(db, APK_BLOB_PTR_LEN(atctx.path, atctx.pathlen));
			r |= apk_dir_foreach_file(
				openat(db->root_fd, argv[i], O_RDONLY|O_CLOEXEC),
				audit_directory_tree_item, &atctx);
			apk_db_dir_unref(db, atctx.dir, FALSE);
		}
	}
	return r;
}

static struct apk_option audit_options[] = {
	{ 0x10000, "backup", "List all modified configuration files (in "
			     "protected_paths.d) that need to be backed up" },
	{ 0x10001, "system", "Verify checksums of all installed non-configuration files " },
	{ 0x10002, "check-permissions", "Check file and directory uid/gid/mode too" },
	{ 'r', "recursive",  "List individually all entries in new directories" },
	{ 0x10003, "packages", "List only the changed packages (or names only with -q)" },
};

static struct apk_applet apk_audit = {
	.name = "audit",
	.help = "Audit the directories (defaults to all) for changes "
		"compared to installed database. Use -q to list only "
		"package names instead of files.",
	.arguments = "[directory to audit]...",
	.open_flags = APK_OPENF_READ|APK_OPENF_NO_SCRIPTS|APK_OPENF_NO_REPOS,
	.context_size = sizeof(struct audit_ctx),
	.num_options = ARRAY_SIZE(audit_options),
	.options = audit_options,
	.parse = audit_parse,
	.main = audit_main,
};

APK_DEFINE_APPLET(apk_audit);

