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
		if (apk_verbosity > 0)
			printf("-%s", pkg->version);
		if (apk_verbosity > 1)
			printf("- %s", pkg->description);
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

static int info_who_owns(struct apk_database *db, int argc, char **argv)
{
	struct apk_package *pkg;
	struct apk_dependency_array *deps = NULL;
	struct apk_dependency dep;
	int i;

	for (i = 0; i < argc; i++) {
		pkg = apk_db_get_file_owner(db, APK_BLOB_STR(argv[i]));
		if (pkg == NULL)
			continue;

		if (apk_verbosity < 1) {
			dep = (struct apk_dependency) {
				.name = pkg->name,
			};
			apk_deps_add(&deps, &dep);
		} else {
			printf("%s is owned by %s-%s\n", argv[i],
			       pkg->name->name, pkg->version);
		}
	}
	if (apk_verbosity < 1 && deps != NULL) {
		char buf[512];
		apk_deps_format(buf, sizeof(buf), deps);
		printf("%s\n", buf);
		free(deps);
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
	case 'W':
		ictx->action = info_who_owns;
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

	if (apk_db_open(&db, apk_root, APK_OPENF_READ) < 0)
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
	{ "who-owns",	no_argument,		NULL, 'W' },
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

