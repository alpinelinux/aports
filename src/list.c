/* list.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2009 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * Copyright (C) 2018 William Pitcock <nenolod@dereferenced.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_print.h"

struct list_ctx {
	unsigned int installed : 1;
	unsigned int orphaned : 1;
	unsigned int available : 1;
	unsigned int upgradable : 1;
	unsigned int match_origin : 1;
	unsigned int match_depends : 1;

	struct apk_string_array *filters;
};

static int origin_matches(const struct list_ctx *ctx, const struct apk_package *pkg)
{
	char **pmatch;

	if (pkg->origin == NULL)
		return 0;

	foreach_array_item(pmatch, ctx->filters)
	{
		if (apk_blob_compare(APK_BLOB_STR(*pmatch), *pkg->origin) == 0)
			return 1;
	}

	return 0;
}

static int is_orphaned(const struct apk_name *name)
{
	struct apk_provider *p;
	unsigned int repos = 0;

	if (name == NULL)
		return 0;

	foreach_array_item(p, name->providers)
		repos |= p->pkg->repos;

	/* repo 1 is always installed-db, so if other bits are set it means the package is available somewhere
	 * (either cache or in a proper repo)
	 */
	return (repos & ~BIT(1)) == 0;
}

/* returns the currently installed package if there is a newer package that satisfies `name` */
static const struct apk_package *is_upgradable(struct apk_name *name, const struct apk_package *pkg0)
{
	struct apk_provider *p;
	struct apk_package *ipkg;
	apk_blob_t *latest = apk_blob_atomize(APK_BLOB_STR(""));

	if (name == NULL)
		return NULL;

	ipkg = apk_pkg_get_installed(name);
	if (ipkg == NULL)
		return NULL;

	if (pkg0 == NULL)
	{
		foreach_array_item(p, name->providers)
		{
			pkg0 = p->pkg;
			int r;

			if (pkg0 == ipkg)
				continue;

			r = apk_version_compare_blob(*pkg0->version, *latest);
			if (r == APK_VERSION_GREATER)
				latest = pkg0->version;
		}
	}
	else
		latest = pkg0->version;

	return apk_version_compare_blob(*ipkg->version, *latest) == APK_VERSION_LESS ? ipkg : NULL;
}

static void print_package(const struct apk_package *pkg, const struct list_ctx *ctx)
{
	printf(PKG_VER_FMT " " BLOB_FMT " {" BLOB_FMT "} (" BLOB_FMT ")",
		PKG_VER_PRINTF(pkg), BLOB_PRINTF(*pkg->arch), BLOB_PRINTF(*pkg->origin),
		BLOB_PRINTF(*pkg->license));

	if (pkg->ipkg)
		printf(" [installed]");
	else
	{
		const struct apk_package *u;

		u = is_upgradable(pkg->name, pkg);
		if (u != NULL)
			printf(" [upgradable from: " PKG_VER_FMT "]", PKG_VER_PRINTF(u));
	}


	if (apk_verbosity > 1)
	{
		printf("\n  %s\n", pkg->description);
		if (apk_verbosity > 2)
			printf("  <%s>\n", pkg->url);
	}

	printf("\n");
}

static void filter_package(const struct apk_package *pkg, const struct list_ctx *ctx)
{
	if (ctx->match_origin && !origin_matches(ctx, pkg))
		return;

	if (ctx->installed && pkg->ipkg == NULL)
		return;

	if (ctx->orphaned && !is_orphaned(pkg->name))
		return;

	if (ctx->available && pkg->repos == BIT(1))
		return;

	if (ctx->upgradable && !is_upgradable(pkg->name, pkg))
		return;

	print_package(pkg, ctx);
}

static void iterate_providers(const struct apk_name *name, const struct list_ctx *ctx)
{
	struct apk_provider *p;

	foreach_array_item(p, name->providers)
	{
		if (p->pkg->name != name)
			continue;

		filter_package(p->pkg, ctx);
	}
}

static void print_result(struct apk_database *db, const char *match, struct apk_name *name, void *pctx)
{
	struct list_ctx *ctx = pctx;

	if (name == NULL)
		return;

	if (ctx->match_depends)
	{
		struct apk_name **pname;

		foreach_array_item(pname, name->rdepends)
			iterate_providers(*pname, ctx);
	}
	else
		iterate_providers(name, ctx);
}

static int option_parse_applet(void *pctx, struct apk_db_options *dbopts, int optch, const char *optarg)
{
	struct list_ctx *ctx = pctx;

	switch (optch)
	{
	case 'I':
		ctx->installed = 1;
		break;
	case 'O':
		ctx->installed = 1;
		ctx->orphaned = 1;
		break;
	case 'u':
		ctx->available = 1;
		ctx->orphaned = 0;
		ctx->installed = 0;
		ctx->upgradable = 1;
		break;
	case 'a':
		ctx->available = 1;
		ctx->orphaned = 0;
		break;
	case 'o':
		ctx->match_origin = 1;
		break;
	case 'd':
		ctx->match_depends = 1;
		break;
	default:
		return -1;
	}

	return 0;
}

static const struct apk_option options_applet[] = {
	{ 'I', "installed", "List installed packages only" },
	{ 'O', "orphaned", "List orphaned packages only" },
	{ 'a', "available", "List available packages only" },
	{ 'u', "upgradable", "List upgradable packages only" },
	{ 'o', "origin", "List packages by origin" },
	{ 'd', "depends", "List packages by dependency" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "List",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

static int list_main(void *pctx, struct apk_database *db, struct apk_string_array *args)
{
	struct list_ctx *ctx = pctx;

	ctx->filters = args;

	if (ctx->match_origin)
		args = NULL;

	apk_name_foreach_matching(
		db, args, APK_FOREACH_NULL_MATCHES_ALL | apk_foreach_genid(),
		print_result, ctx);

	return 0;
}

static struct apk_applet apk_list = {
	.name = "list",
	.help = "List packages by PATTERN and other criteria",
	.arguments = "PATTERN",
	.open_flags = APK_OPENF_READ,
	.command_groups = APK_COMMAND_GROUP_QUERY,
	.context_size = sizeof(struct list_ctx),
	.optgroups = { &optgroup_global, &optgroup_applet },
	.main = list_main,
};

APK_DEFINE_APPLET(apk_list);
