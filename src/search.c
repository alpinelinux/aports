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

#include <fnmatch.h>
#include <stdio.h>
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_state.h"

struct search_ctx {
	int (*action)(struct apk_database *db, int argc, char **argv);
};

struct search_query_ctx {
	struct apk_database *db;
	const char *query;
};

static int search_list_print(apk_hash_item item, void *ctx)
{
	struct apk_package *pkg = (struct apk_package *) item;

	printf("%s", pkg->name->name);
	if (apk_verbosity > 0)
		printf("-%s", pkg->version);
	if (apk_verbosity > 1) {
		printf(" - %s", pkg->description);
	}
	printf("\n");

	return 0;
}

static int search_query_print(apk_hash_item item, void *ctx)
{
	struct search_query_ctx *ictx = (struct search_query_ctx *) ctx;
	struct apk_package *pkg = (struct apk_package *) item;

	if (fnmatch(ictx->query, pkg->name->name, 0) != 0)
		return 0;
	search_list_print(item, ictx->db);

	return 0;
}

static int search_list(struct apk_database *db, int argc, char **argv)
{
	int i;
	struct search_query_ctx ctx;

	ctx.db = db;

	if (argc == 0)
		apk_hash_foreach(&db->available.packages, search_list_print, db);
	else
		for (i=0; i<argc; i++) {
			ctx.query = argv[i];
			apk_hash_foreach(&db->available.packages, search_query_print, &ctx);
		}

	return 0;
}

static int search_query_desc_print(apk_hash_item item, void *ctx)
{
	struct search_query_ctx *ictx = (struct search_query_ctx *) ctx;
	struct apk_package *pkg = (struct apk_package *) item;

	if( strstr(pkg->description, ictx->query) == NULL )
		return 0;
	search_list_print(item, ictx->db);

	return 0;
}

static int search_desc(struct apk_database *db, int argc, char **argv)
{
	int i;
	struct search_query_ctx ctx;

	ctx.db = db;

	for (i=0; i<argc; i++) {
		ctx.query = argv[i];
		apk_hash_foreach(&db->available.packages, search_query_desc_print, &ctx);
	}

	return 0;
}

static int search_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;

	switch (optch) {
	case 'd':
		ictx->action = search_desc;
		break;
	default:
		return -1;
	}
	return 0;
}

static int search_main(void *ctx, int argc, char **argv)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;
	struct apk_database db;
	int r;

	if (apk_db_open(&db, apk_root, APK_OPENF_READ + APK_OPENF_EMPTY_STATE) < 0)
		return -1;

	if (ictx->action != NULL)
		r = ictx->action(&db, argc, argv);
	else
		r = search_list(&db, argc, argv);

	apk_db_close(&db);
	return r;
}

static struct apk_option search_options[] = {
	{ 'd', "description", "Search also package descriptions" },
};

static struct apk_applet apk_search = {
	.name = "search",
	.help = "Search package names (and descriptions) by wildcard PATTERN.",
	.arguments = "PATTERN",
	.context_size = sizeof(struct search_ctx),
	.num_options = ARRAY_SIZE(search_options),
	.options = search_options,
	.parse = search_parse,
	.main = search_main,
};

APK_DEFINE_APPLET(apk_search);

