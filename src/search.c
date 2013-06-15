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

	unsigned int matches;
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
	printf("\n");
}

static void print_origin_name(struct search_ctx *ctx, struct apk_package *pkg)
{
	if (pkg->origin != NULL)
		printf(BLOB_FMT, BLOB_PRINTF(*pkg->origin));
	else
		printf("%s", pkg->name->name);
	if (apk_verbosity > 0)
		printf("-" BLOB_FMT, BLOB_PRINTF(*pkg->version));
	printf("\n");
}

static void print_rdep_pkg(struct apk_package *pkg0, struct apk_dependency *dep0, struct apk_package *pkg, void *pctx)
{
	struct search_ctx *ctx = (struct search_ctx *) pctx;
	ctx->print_package(ctx, pkg0);
}

static void print_rdepends(struct search_ctx *ctx, struct apk_package *pkg)
{
	if (apk_verbosity > 0) {
		ctx->matches = apk_foreach_genid() | APK_DEP_SATISFIES;
		printf(PKG_VER_FMT " is required by:\n", PKG_VER_PRINTF(pkg));
	}
	apk_pkg_foreach_reverse_dependency(pkg, ctx->matches, print_rdep_pkg, ctx);
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

static void print_result_pkg(struct search_ctx *ctx, struct apk_package *pkg)
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
}

static void print_result(struct search_ctx *ctx, struct apk_name *name)
{
	struct apk_provider *p;
	struct apk_package *pkg = NULL;
	apk_blob_t *version = NULL;

	if (ctx->show_all) {
		foreach_array_item(p, name->providers)
			print_result_pkg(ctx, p->pkg);
	} else {
		foreach_array_item(p, name->providers) {
			if (version == NULL ||
			    apk_version_compare_blob(*p->version, *version) == APK_VERSION_GREATER)
				pkg = p->pkg;
		}
		print_result_pkg(ctx, pkg);
	}
}

static int match_names(apk_hash_item item, void *pctx)
{
	struct search_ctx *ctx = (struct search_ctx *) pctx;
	struct apk_name *name = (struct apk_name *) item;
	int i;

	if (!ctx->search_description) {
		for (i = 0; i < ctx->argc; i++)
			if (fnmatch(ctx->argv[i], name->name, FNM_CASEFOLD) == 0)
				break;
		if (ctx->argc > 0 && i >= ctx->argc)
			return 0;
	}
	print_result(ctx, name);

	return 0;
}

static int search_main(void *pctx, struct apk_database *db, int argc, char **argv)
{
	struct search_ctx *ctx = (struct search_ctx *) pctx;
	char s[256];
	int i, l;

	ctx->matches = apk_foreach_genid() | APK_DEP_SATISFIES;
	if (ctx->print_package == NULL)
		ctx->print_package = print_package_name;
	if (ctx->print_result == NULL)
		ctx->print_result = ctx->print_package;

	if (argc == 0 && ctx->search_description)
		return -1;

	ctx->argc = argc;
	if (!ctx->search_exact) {
		ctx->argv = alloca(argc * sizeof(char*));
		for (i = 0; i < argc; i++) {
			l = snprintf(s, sizeof(s), "*%s*", argv[i]);
			ctx->argv[i] = alloca(l+1);
			memcpy(ctx->argv[i], s, l);
			ctx->argv[i][l] = 0;
		}
	} else {
		ctx->argv = argv;
		if (!ctx->search_description) {
			for (i = 0; i < argc; i++)
				print_result(ctx, apk_db_get_name(db, APK_BLOB_STR(argv[i])));
			return 0;
		}
	}

	return apk_hash_foreach(&db->available.names, match_names, ctx);
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
