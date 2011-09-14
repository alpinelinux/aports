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
	void (*print_result)(struct search_ctx *ctx, struct apk_package *pkg);
	void (*print_package)(struct search_ctx *ctx, struct apk_package *pkg);

	int show_all : 1;
	int search_exact : 1;
	int search_description : 1;

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

static int search_parse(void *ctx, struct apk_db_options *dbopts,
		        int optch, int optindex, const char *optarg)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;

	switch (optch) {
	case 'a':
		ictx->show_all = 1;
		break;
	case 'd':
		ictx->search_description = 1;
		ictx->search_exact = 1;
		ictx->show_all = 1;
		break;
	case 'e':
		ictx->search_exact = 1;
		break;
	case 'o':
		ictx->print_package = print_origin_name;
		break;
	case 'r':
		ictx->print_result = print_rdepends;
		break;
	default:
		return -1;
	}
	return 0;
}

static void print_result(struct search_ctx *ctx, struct apk_package *pkg)
{
	int i;

	if (pkg == NULL)
		return;

	if (ctx->search_description) {
		for (i = 0; i < ctx->argc; i++) {
			if (strstr(pkg->description, ctx->argv[i]) != NULL ||
			    strstr(pkg->name->name, ctx->argv[i]) != NULL)
				break;
		}
		if (i >= ctx->argc)
			return;
	}

	ctx->print_result(ctx, pkg);
	printf("\n");
}

static int match_names(apk_hash_item item, void *ctx)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;
	struct apk_name *name = (struct apk_name *) item;
	int i;

	if (!ictx->search_description) {
		for (i = 0; i < ictx->argc; i++)
			if (fnmatch(ictx->argv[i], name->name, FNM_CASEFOLD) == 0)
				break;
		if (ictx->argc > 0 && i >= ictx->argc)
			return 0;
	}

	if (ictx->show_all) {
		for (i = 0; i < name->pkgs->num; i++)
			print_result(ictx, name->pkgs->item[i]);
	} else {
		struct apk_package *pkg = NULL;

		for (i = 0; i < name->pkgs->num; i++) {
			if (pkg == NULL ||
			    apk_pkg_version_compare(name->pkgs->item[i], pkg) == APK_VERSION_GREATER)
				pkg = name->pkgs->item[i];
		}
		print_result(ictx, pkg);
	}
	return 0;
}

static int search_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;
	char s[256];
	int i, l;

	if (ictx->print_package == NULL)
		ictx->print_package = print_package_name;
	if (ictx->print_result == NULL)
		ictx->print_result = ictx->print_package;

	if (argc == 0 && ictx->search_description)
		return -1;

	ictx->argc = argc;
	if (!ictx->search_exact) {
		ictx->argv = alloca(argc * sizeof(char*));
		for (i = 0; i < argc; i++) {
			l = snprintf(s, sizeof(s), "*%s*", argv[i]);
			ictx->argv[i] = alloca(l+1);
			memcpy(ictx->argv[i], s, l);
			ictx->argv[i][l] = 0;
		}
	} else {
		ictx->argv = argv;
	}

	return apk_hash_foreach(&db->available.names, match_names, ictx);
}

static struct apk_option search_options[] = {
	{ 'a', "all",		"Show all package versions (instead of latest only)" },
	{ 'd', "description",	"Search package descriptions (implies -a)" },
	{ 'e', "exact",		"Require exact match (instead of substring match)" },
	{ 'o', "origin",	"Print origin package name instead of the subpackage" },
	{ 'r', "rdepends",	"Print reverse dependencies of package" },
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

