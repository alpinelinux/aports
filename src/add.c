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

#define FLAG_INITDB		0x001

struct add_ctx {
	unsigned int flags;
};

static int add_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct add_ctx *actx = (struct add_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		actx->flags |= FLAG_INITDB;
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
	int i, r, ret = 1;

	r = apk_db_open(&db, apk_root);
	if ((r == -ENOENT) && (actx->flags & FLAG_INITDB)) {
		if (strcmp(apk_root, "/") == 0) {
			apk_error("Will not recreate system root.");
			return 1;
		}
		r = apk_db_create(apk_root);
		if (r != 0) {
			apk_error("Failed to create apkdb: %s",
				  strerror(-r));
			return 1;
		}
		r = apk_db_open(&db, apk_root);
	}
	if (r != 0) {
		apk_error("APK database not present (use --initdb to create one)");
		return 1;
	}

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
				.version_mask = APK_VERSION_RESULT_MASK(APK_VERSION_EQUAL),
				.version = pkg->version,
			};
		} else {
			dep = (struct apk_dependency) {
				.name = apk_db_get_name(&db, APK_BLOB_STR(argv[i])),
			};
		}
		apk_deps_add(&db.world, &dep);
	}
	ret = apk_db_recalculate_and_commit(&db);
err:
	apk_db_close(&db);
	return ret;
}

static struct option add_options[] = {
	{ "initdb",	no_argument,		NULL, 0x10000 },
};

static struct apk_applet apk_add = {
	.name = "add",
	.usage = "[--initdb] apkname...",
	.context_size = sizeof(struct add_ctx),
	.num_options = ARRAY_SIZE(add_options),
	.options = add_options,
	.parse = add_parse,
	.main = add_main,
};

APK_DEFINE_APPLET(apk_add);

