/* add.c - Alpine Package Keeper (APK)
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

struct add_ctx {
	unsigned int open_flags;
};

static int add_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct add_ctx *actx = (struct add_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		actx->open_flags |= APK_OPENF_CREATE;
		break;
	case 'u':
		apk_flags |= APK_UPGRADE;
		break;
	default:
		return -1;
	}
	return 0;
}

static int add_main(void *ctx, int argc, char **argv)
{
	struct add_ctx *actx = (struct add_ctx *) ctx;
	struct apk_database db;
	struct apk_state *state;
	int i, r;

	r = apk_db_open(&db, apk_root, actx->open_flags | APK_OPENF_WRITE);
	if (r != 0)
		return r;

	state = apk_state_new(&db);
	for (i = 0; i < argc; i++) {
		struct apk_dependency dep;

		if (strstr(argv[i], ".apk") != NULL) {
			struct apk_package *pkg;

			pkg = apk_db_pkg_add_file(&db, argv[i]);
			if (pkg == NULL) {
				apk_error("Unable to read '%s'", argv[i]);
				goto err;
			}

			dep = (struct apk_dependency) {
				.name = pkg->name,
				.version = pkg->version,
				.result_mask = APK_VERSION_EQUAL,
			};
		} else {
			dep = (struct apk_dependency) {
				.name = apk_db_get_name(&db, APK_BLOB_STR(argv[i])),
				.result_mask = APK_DEPMASK_REQUIRE,
			};
		}
		apk_deps_add(&db.world, &dep);
		dep.name->flags |= APK_NAME_TOPLEVEL;

		r = apk_state_lock_dependency(state, &dep);
		if (r != 0) {
			apk_error("Unable to install '%s'", dep.name->name);
			goto err;
		}
	}
	r = apk_state_commit(state, &db);
err:
	apk_state_unref(state);
	apk_db_close(&db);
	return r;
}

static struct option add_options[] = {
	{ "initdb",	no_argument,		NULL, 0x10000 },
	{ "upgrade",	no_argument,		NULL, 'u' },
};

static struct apk_applet apk_add = {
	.name = "add",
	.usage = "[--initdb] [--upgrade|-u] apkname...",
	.context_size = sizeof(struct add_ctx),
	.num_options = ARRAY_SIZE(add_options),
	.options = add_options,
	.parse = add_parse,
	.main = add_main,
};

APK_DEFINE_APPLET(apk_add);

