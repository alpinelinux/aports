/* fix.c - Alpine Package Keeper (APK)
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
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_print.h"
#include "apk_solver.h"

/* FIXME: reimplement fix applet */

struct fix_ctx {
	unsigned short solver_flags;
};

static int fix_parse(void *pctx, struct apk_db_options *dbopts,
		     int optch, int optindex, const char *optarg)
{
	struct fix_ctx *ctx = (struct fix_ctx *) pctx;
	switch (optch) {
	case 'u':
		ctx->solver_flags |= APK_SOLVERF_UPGRADE;
		break;
	case 'r':
		ctx->solver_flags |= APK_SOLVERF_REINSTALL;
		break;
	default:
		return -1;
	}
	return 0;
}

static int fix_main(void *pctx, struct apk_database *db, int argc, char **argv)
{
	struct fix_ctx *ctx = (struct fix_ctx *) pctx;
	struct apk_name *name;
	struct apk_package *pkg;
	int r = 0, i, j;

	if (!ctx->solver_flags)
		ctx->solver_flags = APK_SOLVERF_REINSTALL;

	for (i = 0; i < argc; i++) {
		pkg = NULL;
		if (strstr(argv[i], ".apk") != NULL) {
			struct apk_sign_ctx sctx;

			apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY_AND_GENERATE,
					  NULL, db->keys_fd);
			r = apk_pkg_read(db, argv[i], &sctx, &pkg);
			apk_sign_ctx_free(&sctx);
			if (r != 0) {
				apk_error("%s: %s", argv[i], apk_error_str(r));
				goto err;
			}
			name = pkg->name;
		} else {
			name = apk_db_get_name(db, APK_BLOB_STR(argv[i]));
			for (j = 0; j < name->pkgs->num; j++) {
				if (name->pkgs->item[j]->ipkg != NULL) {
					pkg = name->pkgs->item[j];
					break;
				}
			}
		}
		if (pkg == NULL || pkg->ipkg == NULL) {
			apk_error("%s is not installed", name->name);
			goto err;
		}
		apk_solver_set_name_flags(name, ctx->solver_flags);
	}

	r = apk_solver_commit(db, 0, db->world);
err:
	return r;
}

static struct apk_option fix_options[] = {
	{ 'u',		"upgrade",	"Upgrade package if possible" },
	{ 'r',		"reinstall",	"Reinstall the package" },
};

static struct apk_applet apk_fix = {
	.name = "fix",
	.help = "Repair package or upgrade it without modifying main "
		"dependencies.",
	.arguments = "PACKAGE...",
	.open_flags = APK_OPENF_WRITE,
	.context_size = sizeof(struct fix_ctx),
	.num_options = ARRAY_SIZE(fix_options),
	.options = fix_options,
	.parse = fix_parse,
	.main = fix_main,
};

APK_DEFINE_APPLET(apk_fix);

