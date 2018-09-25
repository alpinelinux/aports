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

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>

#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_package.h"
#include "apk_print.h"
#include "apk_solver.h"

#define CACHE_CLEAN	BIT(0)
#define CACHE_DOWNLOAD	BIT(1)

struct cache_ctx {
	unsigned short solver_flags;
};

static int option_parse_applet(void *ctx, struct apk_db_options *dbopts, int optch, const char *optarg)
{
	struct cache_ctx *cctx = (struct cache_ctx *) ctx;

	switch (optch) {
	case 'u':
		cctx->solver_flags |= APK_SOLVERF_UPGRADE;
		break;
	case 'l':
		cctx->solver_flags |= APK_SOLVERF_LATEST;
		break;
	default:
		return -ENOTSUP;
	}
	return 0;
}

static const struct apk_option options_applet[] = {
	{ 'u',		"upgrade",	"Prefer to upgrade package" },
	{ 'l',		"latest",
	  "Select latest version of package (if it is not pinned), and "
	  "print error if it cannot be installed due to other dependencies" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "Cache",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

struct progress {
	size_t done, total;
};

static void progress_cb(void *ctx, size_t bytes_done)
{
	struct progress *prog = (struct progress *) ctx;
	apk_print_progress(prog->done + bytes_done, prog->total);
}

static int cache_download(struct cache_ctx *cctx, struct apk_database *db)
{
	struct apk_changeset changeset = {};
	struct apk_change *change;
	struct apk_package *pkg;
	struct apk_repository *repo;
	struct progress prog = { 0, 0 };
	int r, ret = 0;

	r = apk_solver_solve(db, cctx->solver_flags, db->world, &changeset);
	if (r < 0) {
		apk_error("Unable to select packages. Run apk fix.");
		return r;
	}

	foreach_array_item(change, changeset.changes) {
		pkg = change->new_pkg;
		if ((pkg != NULL) && !(pkg->repos & db->local_repos))
			prog.total += pkg->size;
	}

	foreach_array_item(change, changeset.changes) {
		pkg = change->new_pkg;
		if ((pkg == NULL) || (pkg->repos & db->local_repos))
			continue;

		repo = apk_db_select_repo(db, pkg);
		if (repo == NULL)
			continue;

		r = apk_cache_download(db, repo, pkg, APK_SIGN_VERIFY_IDENTITY, 0,
				       progress_cb, &prog);
		if (r && r != -EALREADY) {
			apk_error(PKG_VER_FMT ": %s", PKG_VER_PRINTF(pkg), apk_error_str(r));
			ret++;
		}
		prog.done += pkg->size;
	}

	return ret;
}

static void cache_clean_item(struct apk_database *db, int dirfd, const char *name, struct apk_package *pkg)
{
	char tmp[PATH_MAX];
	apk_blob_t b;
	int i;

	if (strcmp(name, "installed") == 0) return;

	if (pkg) {
		if ((apk_flags & APK_PURGE) && pkg->ipkg == NULL) goto delete;
		if (pkg->repos & db->local_repos & ~BIT(APK_REPOSITORY_CACHED)) goto delete;
		if (pkg->ipkg == NULL && !(pkg->repos & ~BIT(APK_REPOSITORY_CACHED))) goto delete;
		return;
	}

	b = APK_BLOB_STR(name);
	for (i = 0; i < db->num_repos; i++) {
		/* Check if this is a valid index */
		apk_repo_format_cache_index(APK_BLOB_BUF(tmp), &db->repos[i]);
		if (apk_blob_compare(b, APK_BLOB_STR(tmp)) == 0) return;
	}

delete:
	if (apk_verbosity >= 2)
		apk_message("deleting %s", name);
	if (!(apk_flags & APK_SIMULATE)) {
		if (unlinkat(dirfd, name, 0) < 0 && errno == EISDIR)
			unlinkat(dirfd, name, AT_REMOVEDIR);
	}
}

static int cache_clean(struct apk_database *db)
{
	return apk_db_cache_foreach_item(db, cache_clean_item);
}

static int cache_main(void *ctx, struct apk_database *db, struct apk_string_array *args)
{
	struct cache_ctx *cctx = (struct cache_ctx *) ctx;
	char *arg;
	int r = 0, actions = 0;

	if (args->num != 1)
		return -EINVAL;

	arg = args->item[0];
	if (strcmp(arg, "sync") == 0)
		actions = CACHE_CLEAN | CACHE_DOWNLOAD;
	else if (strcmp(arg, "clean") == 0)
		actions = CACHE_CLEAN;
	else if (strcmp(arg, "download") == 0)
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
		r = cache_download(cctx, db);
err:
	return r;
}

static struct apk_applet apk_cache = {
	.name = "cache",
	.help = "Download missing PACKAGEs to cache and/or delete "
		"unneeded files from cache",
	.arguments = "sync | clean | download",
	.open_flags = APK_OPENF_READ|APK_OPENF_NO_SCRIPTS|APK_OPENF_CACHE_WRITE,
	.command_groups = APK_COMMAND_GROUP_SYSTEM,
	.context_size = sizeof(struct cache_ctx),
	.optgroups = { &optgroup_global, &optgroup_applet },
	.main = cache_main,
};

APK_DEFINE_APPLET(apk_cache);
