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
#include <limits.h>
#include <sys/stat.h>
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_print.h"

/* Use (unused) highest bit of mode_t as seen flag of our internal
 * database file entries */
#define S_SEENFLAG	0x80000000

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

static int option_parse_applet(void *ctx, struct apk_db_options *dbopts, int optch, const char *optarg)
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
		return -ENOTSUP;
	}
	return 0;
}

static const struct apk_option options_applet[] = {
	{ 0x10000, "backup", "List all modified configuration files (in "
			     "protected_paths.d) that need to be backed up" },
	{ 0x10001, "system", "Verify checksums of all installed non-configuration files " },
	{ 0x10002, "check-permissions", "Check file and directory uid/gid/mode too" },
	{ 'r', "recursive",  "List individually all entries in new directories" },
	{ 0x10003, "packages", "List only the changed packages (or names only with -q)" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "Audit",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

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
	int rv = 0;

	if (dbf == NULL)
		return 'A';

	dbf->audited = 1;

	if (apk_fileinfo_get(dirfd, name,
				APK_FI_NOFOLLOW |
				APK_FI_XATTR_CSUM(dbf->acl->xattr_csum.type ?: APK_CHECKSUM_DEFAULT) |
				APK_FI_CSUM(dbf->csum.type),
				&fi) != 0)
		return -EPERM;

	if (dbf->csum.type != APK_CHECKSUM_NONE &&
	    apk_checksum_compare(&fi.csum, &dbf->csum) != 0)
		rv = 'U';
	else if (!S_ISLNK(fi.mode) && !dbf->diri->pkg->ipkg->broken_xattr &&
	         apk_checksum_compare(&fi.xattr_csum, &dbf->acl->xattr_csum) != 0)
		rv = 'x';
	else if (S_ISLNK(fi.mode) && dbf->csum.type == APK_CHECKSUM_NONE)
		rv = 'U';
	else if (actx->check_permissions) {
		if ((fi.mode & 07777) != (dbf->acl->mode & 07777))
			rv = 'M';
		else if (fi.uid != dbf->acl->uid || fi.gid != dbf->acl->gid)
			rv = 'M';
	}
	apk_fileinfo_free(&fi);

	return rv;
}

static int audit_directory(struct audit_ctx *actx,
			   struct apk_database *db,
			   struct apk_db_dir *dbd,
			   struct apk_file_info *fi)
{
	if (dbd != NULL) dbd->mode |= S_SEENFLAG;

	if (dbd == NULL || dbd->refs == 1)
		return actx->recursive ? 'd' : 'D';

	if (actx->check_permissions &&
	    ((dbd->mode & ~S_SEENFLAG) || dbd->uid || dbd->gid)) {
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

	if (bdir.len + bent.len + 1 >= sizeof(atctx->path)) return 0;
	if (apk_fileinfo_get(dirfd, name, APK_FI_NOFOLLOW, &fi) < 0) return 0;

	memcpy(&atctx->path[atctx->pathlen], bent.ptr, bent.len);
	atctx->pathlen += bent.len;
	bfull = APK_BLOB_PTR_LEN(atctx->path, atctx->pathlen);

	if (S_ISDIR(fi.mode)) {
		int recurse = TRUE;

		if (actx->mode == MODE_BACKUP) {
			child = apk_db_dir_get(db, bfull);
			if (!child->has_protected_children)
				recurse = FALSE;
			if (child->protect_mode == APK_PROTECT_NONE)
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
		struct apk_protected_path *ppath;
		int protect_mode = dir->protect_mode;

		/* inherit file's protection mask */
		foreach_array_item(ppath, dir->protected_paths) {
			char *slash = strchr(ppath->relative_pattern, '/');
			if (slash == NULL) {
				if (fnmatch(ppath->relative_pattern, name, FNM_PATHNAME) != 0)
					continue;
				protect_mode = ppath->protect_mode;
			}
		}

		if (actx->mode == MODE_BACKUP) {
			switch (protect_mode) {
			case APK_PROTECT_NONE:
				goto done;
			case APK_PROTECT_CHANGED:
				break;
			case APK_PROTECT_SYMLINKS_ONLY:
				if (!S_ISLNK(fi.mode))
					goto done;
				break;
			case APK_PROTECT_ALL:
				reason = 'A';
				break;
			}
		}

		dbf = apk_db_file_query(db, bdir, bent);
		if (reason == 0)
			reason = audit_file(actx, db, dbf, dirfd, name);
		if (reason < 0)
			goto done;
		if (actx->mode == MODE_SYSTEM &&
		    (reason == 'A' || protect_mode != APK_PROTECT_NONE))
			goto done;
		if (actx->mode == MODE_BACKUP &&
		    reason == 'A' &&
		    apk_blob_ends_with(bent, APK_BLOB_STR(".apk-new")))
			goto done;
		report_audit(actx, reason, bfull, dbf ? dbf->diri->pkg : NULL);
	}

done:
	if (child)
		apk_db_dir_unref(db, child, FALSE);

	atctx->pathlen -= bent.len;
	return 0;
}

static int audit_directory_tree(struct audit_tree_ctx *atctx, int dirfd)
{
	apk_blob_t path;
	int r;

	path = APK_BLOB_PTR_LEN(atctx->path, atctx->pathlen);
	if (path.len && path.ptr[path.len-1] == '/')
		path.len--;

	atctx->dir = apk_db_dir_get(atctx->db, path);
	atctx->dir->mode |= S_SEENFLAG;
	r = apk_dir_foreach_file(dirfd, audit_directory_tree_item, atctx);
	apk_db_dir_unref(atctx->db, atctx->dir, FALSE);

	return r;
}

static int audit_missing_files(apk_hash_item item, void *pctx)
{
	struct audit_ctx *actx = pctx;
	struct apk_db_file *file = item;
	struct apk_db_dir *dir;
	char path[PATH_MAX];
	int len;

	if (file->audited) return 0;

	dir = file->diri->dir;
	if (dir->mode & S_SEENFLAG) {
		len = snprintf(path, sizeof(path), DIR_FILE_FMT, DIR_FILE_PRINTF(dir, file));
		report_audit(actx, 'X', APK_BLOB_PTR_LEN(path, len), file->diri->pkg);
	}

	return 0;
}

static int audit_main(void *ctx, struct apk_database *db, struct apk_string_array *args)
{
	struct audit_tree_ctx atctx;
	struct audit_ctx *actx = (struct audit_ctx *) ctx;
	char **parg, *arg;
	int r = 0;

	atctx.db = db;
	atctx.actx = actx;
	atctx.pathlen = 0;
	atctx.path[0] = 0;

	if (args->num == 0) {
		r |= audit_directory_tree(&atctx, dup(db->root_fd));
	} else {
		foreach_array_item(parg, args) {
			arg = *parg;
			if (arg[0] != '/') {
				apk_warning("%s: relative path skipped.\n", arg);
				continue;
			}
			arg++;
			atctx.pathlen = strlen(arg);
			memcpy(atctx.path, arg, atctx.pathlen);
			if (atctx.path[atctx.pathlen-1] != '/')
				atctx.path[atctx.pathlen++] = '/';

			r |= audit_directory_tree(&atctx, openat(db->root_fd, arg, O_RDONLY|O_CLOEXEC));
		}
	}
	if (actx->mode == MODE_SYSTEM)
		apk_hash_foreach(&db->installed.files, audit_missing_files, ctx);

	return r;
}

static struct apk_applet apk_audit = {
	.name = "audit",
	.help = "Audit the directories for changes",
	.arguments = "[directory to audit]...",
	.open_flags = APK_OPENF_READ|APK_OPENF_NO_SCRIPTS|APK_OPENF_NO_REPOS,
	.context_size = sizeof(struct audit_ctx),
	.optgroups = { &optgroup_global, &optgroup_applet },
	.main = audit_main,
};

APK_DEFINE_APPLET(apk_audit);

