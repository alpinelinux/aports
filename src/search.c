/* info.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2009 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <fnmatch.h>
#include <stdio.h>
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_package.h"
#include "apk_database.h"

struct search_ctx {
	int (*match)(struct apk_package *pkg, const char *str);
	void (*print_result)(struct search_ctx *ctx, struct apk_package *pkg);
	void (*print_package)(struct search_ctx *ctx, struct apk_package *pkg);
	int argc;
	char **argv;
};

static void print_package_name(struct search_ctx *ctx, struct apk_package *pkg)
{
	printf("%s", pkg->name->name);
	if (apk_verbosity > 0)
		printf("-" BLOB_FMT, BLOB_PRINTF(*pkg->version));
	if (apk_verbosity > 1)
		printf(" - %s", pkg->description);
}

static void print_origin_name(struct search_ctx *ctx, struct apk_package *pkg)
{
	if (pkg->origin != NULL)
		printf(BLOB_FMT, BLOB_PRINTF(*pkg->origin));
	else
		printf("%s", pkg->name->name);
	if (apk_verbosity > 0)
		printf("-" BLOB_FMT, BLOB_PRINTF(*pkg->version));
}

static void print_rdepends(struct search_ctx *ctx, struct apk_package *pkg)
{
	struct apk_name *name, *name0;
	struct apk_package *pkg0;
	struct apk_dependency *dep;
	int i, j, k;

	name = pkg->name;

	printf(PKG_VER_FMT ":", PKG_VER_PRINTF(pkg));
	for (i = 0; i < name->rdepends->num; i++) {
		name0 = name->rdepends->item[i];
		for (j = 0; j < name0->pkgs->num; j++) {
			pkg0 = name0->pkgs->item[j];
			for (k = 0; k < pkg0->depends->num; k++) {
				dep = &pkg0->depends->item[k];
				if (name == dep->name &&
				    apk_dep_is_satisfied(dep, pkg)) {
					printf(" ");
					ctx->print_package(ctx, pkg0);
				}
			}
		}
	}
}

static int search_pkgname(struct apk_package *pkg, const char *str)
{
	return fnmatch(str, pkg->name->name, 0) == 0;
}

static int search_desc(struct apk_package *pkg, const char *str)
{
	return  strstr(pkg->name->name, str) != NULL ||
		strstr(pkg->description, str) != NULL;
}

static int search_parse(void *ctx, struct apk_db_options *dbopts,
		        int optch, int optindex, const char *optarg)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;

	switch (optch) {
	case 'd':
		ictx->match = search_desc;
		break;
	case 'r':
		ictx->print_result = print_rdepends;
		break;
	case 'o':
		ictx->print_package = print_origin_name;
		break;
	default:
		return -1;
	}
	return 0;
}

static int match_packages(apk_hash_item item, void *ctx)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;
	struct apk_package *pkg = (struct apk_package *) item;
	int i;

	for (i = 0; i < ictx->argc; i++)
		if (ictx->match(pkg, ictx->argv[i]))
			break;
	if (ictx->argc == 0 || i < ictx->argc) {
		ictx->print_result(ictx, pkg);
		printf("\n");
	}

	return 0;
}

static int search_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;
	struct apk_name *name;
	int rc = 0, i, j, slow_search;

	slow_search = ictx->match != NULL || argc == 0;
	if (!slow_search) {
		for (i = 0; i < argc; i++)
			if (strcspn(argv[i], "*?[") != strlen(argv[i])) {
				slow_search = 1;
				break;
			}
	}

	if (ictx->match == NULL)
		ictx->match = search_pkgname;
	if (ictx->print_package == NULL)
		ictx->print_package = print_package_name;
	if (ictx->print_result == NULL)
		ictx->print_result = ictx->print_package;
	else if (argc == 0)
		return -1;

	if (slow_search) {
		ictx->argc = argc;
		ictx->argv = argv;
		rc = apk_hash_foreach(&db->available.packages,
				      match_packages, ictx);
	} else {
		for (i = 0; i < argc; i++) {
			name = apk_db_query_name(db, APK_BLOB_STR(argv[i]));
			if (name == NULL)
				continue;
			for (j = 0; j < name->pkgs->num; j++) {
				ictx->print_result(ctx, name->pkgs->item[j]);
				printf("\n");
			}
		}
	}

	return rc;
}

static struct apk_option search_options[] = {
	{ 'd', "description",	"Search also package descriptions" },
	{ 'r', "rdepends",	"Print reverse dependencies of package" },
	{ 'o', "origin",	"Print origin package name instead of the subpackage" },
};

static struct apk_applet apk_search = {
	.name = "search",
	.help = "Search package by PATTERNs or by indexed dependencies.",
	.arguments = "PATTERN",
	.open_flags = APK_OPENF_READ | APK_OPENF_NO_STATE,
	.context_size = sizeof(struct search_ctx),
	.num_options = ARRAY_SIZE(search_options),
	.options = search_options,
	.parse = search_parse,
	.main = search_main,
};

APK_DEFINE_APPLET(apk_search);

