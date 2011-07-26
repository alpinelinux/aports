/* test.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <errno.h>
#include <fcntl.h>
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_solver.h"
#include "apk_io.h"
#include "apk_print.h"

struct test_ctx {
	const char *installed_db;
	struct apk_string_array *repos;
};

static int test_parse(void *pctx, struct apk_db_options *dbopts,
		      int optch, int optindex, const char *optarg)
{
	struct test_ctx *ctx = (struct test_ctx *) pctx;

	switch (optch) {
	case 0x10000:
		ctx->installed_db = optarg;
		break;
	case 0x10001:
		if (ctx->repos == NULL)
			apk_string_array_init(&ctx->repos);
		*apk_string_array_add(&ctx->repos) = (char*) optarg;
		break;
	case 'u':
		apk_flags |= APK_UPGRADE;
		break;
	case 'a':
		apk_flags |= APK_PREFER_AVAILABLE;
		break;
	default:
		return -1;
	}
	return 0;
}

static inline void print_change(struct apk_package *oldpkg,
				struct apk_package *newpkg)
{
	const char *msg = NULL;
	struct apk_name *name;
	int r;

	if (oldpkg != NULL)
		name = oldpkg->name;
	else
		name = newpkg->name;

	if (oldpkg == NULL) {
		apk_message("Installing %s (" BLOB_FMT ")",
			    name->name,
			    BLOB_PRINTF(*newpkg->version));
	} else if (newpkg == NULL) {
		apk_message("Purging %s (" BLOB_FMT ")",
			    name->name,
			    BLOB_PRINTF(*oldpkg->version));
	} else {
		r = apk_pkg_version_compare(newpkg, oldpkg);
		switch (r) {
		case APK_VERSION_LESS:
			msg = "Downgrading";
			break;
		case APK_VERSION_EQUAL:
			if (newpkg == oldpkg)
				msg = "Re-installing";
			else
				msg = "Replacing";
			break;
		case APK_VERSION_GREATER:
			msg = "Upgrading";
			break;
		}
		apk_message("%s %s (" BLOB_FMT " -> " BLOB_FMT ")",
			    msg, name->name,
			    BLOB_PRINTF(*oldpkg->version),
			    BLOB_PRINTF(*newpkg->version));
	}
}

static int test_main(void *pctx, struct apk_database *db, int argc, char **argv)
{
	struct test_ctx *ctx = (struct test_ctx *) pctx;
	struct apk_bstream *bs;
	struct apk_package_array *solution = NULL;
	struct apk_changeset changeset;
	int i;

	if (argc != 1)
		return -EINVAL;

	/* load installed db */
	if (ctx->installed_db != NULL) {
		bs = apk_bstream_from_file(AT_FDCWD, ctx->installed_db);
		apk_db_index_read(db, bs, -1);
		bs->close(bs, NULL);
	}

	/* load additional indexes */
	if (ctx->repos) {
		for (i = 0; i < ctx->repos->num; i++) {
			bs = apk_bstream_from_file(AT_FDCWD, ctx->repos->item[i]);
			apk_db_index_read(db, bs, i);
			bs->close(bs, NULL);
		}
	}

	/* construct new world */
	apk_deps_parse(db, &db->world, APK_BLOB_STR(argv[0]));

	/* run solver */
	apk_solver_sort(db);
	if (apk_solver_solve(db, db->world, &solution) != 0)
		return 1;

	memset(&changeset, 0, sizeof(changeset));
	if (apk_solver_generate_changeset(db, solution, &changeset) == 0) {
		/* dump changeset */
		for (i = 0; i < changeset.changes->num; i++) {
			struct apk_change *c = &changeset.changes->item[i];
			print_change(c->oldpkg, c->newpkg);
		}
	}

	return 0;
}

static struct apk_option test_options[] = {
	{ 0x10000,	"installed",	"Installed database",
			required_argument, "DB" },
	{ 0x10001,	"raw-repository", "Add unsigned test repository index",
			required_argument, "INDEX" },
	{ 'u',		"upgrade",	"Prefer to upgrade package" },
	{ 'a',		"available",
	  "Re-install or downgrade if currently installed package is not "
	  "currently available from any repository" },
};

static struct apk_applet test_applet = {
	.name = "test",
	.help = "Test dependency graph solver (uses simple repository and installed db)",
	.arguments = "'WORLD'",
	.open_flags = APK_OPENF_READ | APK_OPENF_NO_STATE | APK_OPENF_NO_REPOS,
	.context_size = sizeof(struct test_ctx),
	.num_options = ARRAY_SIZE(test_options),
	.options = test_options,
	.parse = test_parse,
	.main = test_main,
};

APK_DEFINE_APPLET(test_applet);
