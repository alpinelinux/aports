/* fix.c - Alpine Package Keeper (APK)
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
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_state.h"
#include "apk_print.h"

struct fix_ctx {
	unsigned int reinstall : 1;
};

static int fix_parse(void *pctx, struct apk_db_options *dbopts,
		     int optch, int optindex, const char *optarg)
{
	struct fix_ctx *ctx = (struct fix_ctx *) pctx;
	switch (optch) {
	case 'u':
		apk_flags |= APK_UPGRADE;
		break;
	case 'r':
		ctx->reinstall = 1;
		break;
	default:
		return -1;
	}
	return 0;
}

static int fix_main(void *pctx, struct apk_database *db, int argc, char **argv)
{
	struct fix_ctx *ctx = (struct fix_ctx *) pctx;
	struct apk_state *state = NULL;
	struct apk_name *name;
	struct apk_package *pkg;
	struct apk_dependency *deps;
	int r = 0, i, j;

	state = apk_state_new(db);
	if (state == NULL)
		return -1;

	deps = alloca(sizeof(struct apk_dependency) * argc);
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
			apk_dep_from_pkg(&deps[i], db, pkg);
			name = deps[i].name;
		} else {
			apk_dep_from_blob(&deps[i], db, APK_BLOB_STR(argv[i]));
			name = deps[i].name;

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
		if (ctx->reinstall)
			name->flags |= APK_NAME_REINSTALL;
	}

	for (i = 0; i < argc; i++)
		r |= apk_state_lock_dependency(state, &deps[i]);

	if (r == 0 || (apk_flags & APK_FORCE))
		r = apk_state_commit(state, db);
	else
		apk_state_print_errors(state);
err:
	if (r != 0 && i < argc)
		apk_error("Error while processing '%s'", argv[i]);
	if (state != NULL)
		apk_state_unref(state);
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

