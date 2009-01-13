/* info.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2009 Natanael Copa <n@tanael.org>
 * Copyright (C) 2009 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_state.h"

struct info_ctx {
	int (*action)(struct apk_database *db, int argc, char **argv);
};

static int info_list(struct apk_database *db, int argc, char **argv)
{
	struct apk_package *pkg;

	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list) {
		printf("%s", pkg->name->name);
		if (!apk_quiet)
			printf("-%s - %s", pkg->version, pkg->description);
		printf("\n");
	}
	return 0;
}

static int info_exists(struct apk_database *db, int argc, char **argv)
{
	struct apk_name *name;
	int i, j;

	for (i = 0; i < argc; i++) {
		name = apk_db_query_name(db, APK_BLOB_STR(argv[i]));
		if (name == NULL)
			return 1;

		for (j = 0; j < name->pkgs->num; j++) {
			if (apk_pkg_get_state(name->pkgs->item[j]) == APK_STATE_INSTALL)
				break;
		}
		if (j >= name->pkgs->num)
			return 2;
	}

	return 0;
}

static int info_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct info_ctx *ictx = (struct info_ctx *) ctx;

	switch (optch) {
	case 'e':
		ictx->action = info_exists;
		break;
	default:
		return -1;
	}
	return 0;
}

static int info_main(void *ctx, int argc, char **argv)
{
	struct info_ctx *ictx = (struct info_ctx *) ctx;
	struct apk_database db;
	int r;

	if (apk_db_open(&db, apk_root) < 0)
		return -1;

	if (ictx->action != NULL)
		r = ictx->action(&db, argc, argv);
	else
		r = info_list(&db, argc, argv);

	apk_db_close(&db);
	return r;
}

static struct option info_options[] = {
	{ "installed",	no_argument,		NULL, 'e' },
};

static struct apk_applet apk_info = {
	.name = "info",
	.usage = "",
	.context_size = sizeof(struct info_ctx),
	.num_options = ARRAY_SIZE(info_options),
	.options = info_options,
	.parse = info_parse,
	.main = info_main,
};

APK_DEFINE_APPLET(apk_info);

