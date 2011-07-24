/* dot.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include <fnmatch.h>

#include "apk_applet.h"
#include "apk_database.h"
#include "apk_print.h"

#define S_EVALUATED	-1
#define S_EVALUATING	-2

struct dot_ctx {
	int errors_only;
	int not_empty;
};

static int dot_parse(void *pctx, struct apk_db_options *dbopts,
		     int optch, int optindex, const char *optarg)
{
	struct dot_ctx *ctx = (struct dot_ctx *) pctx;

	switch (optch) {
	case 0x10000:
		ctx->errors_only = 1;
		break;
	default:
		return -1;
	}
	return 0;
}

static void start_graph(struct dot_ctx *ctx)
{
	if (ctx->not_empty)
		return;
	ctx->not_empty = 1;

	printf( "digraph \"apkindex\" {\n"
		"  rankdir=LR;\n"
		"  node [shape=box];\n");
}

static int dump_pkg(struct dot_ctx *ctx, struct apk_package *pkg)
{
	int i, j, r, ret = 0;

	/* Succesfully vandalize the apk_package by reusing
	 * size field for our evil purposes ;) */
	if (pkg->size == S_EVALUATED)
		return 0;
	if (((ssize_t)pkg->size) <= S_EVALUATING) {
		pkg->size--;
		return 1;
	}

	pkg->size = S_EVALUATING;
	for (i = 0; i < pkg->depends->num; i++) {
		struct apk_dependency *dep = &pkg->depends->item[i];
		struct apk_name *name = dep->name;

		if (name->pkgs->num == 0) {
			start_graph(ctx);
			printf("  \"" PKG_VER_FMT "\" -> \"%s\" [color=red];\n",
				PKG_VER_PRINTF(pkg),
				name->name);
			if (!(name->flags & APK_NAME_VISITED)) {
				printf("  \"%s\" [style=dashed, color=red, fontcolor=red, shape=octagon];\n",
					name->name);
				name->flags |= APK_NAME_VISITED;
			}
		} else {
			for (j = 0; j < name->pkgs->num; j++) {
				struct apk_package *pkg0 = name->pkgs->item[j];

				if (!apk_dep_is_satisfied(dep, pkg0))
					continue;

				r = dump_pkg(ctx, pkg0);
				ret += r;
				if (r || (!ctx->errors_only)) {
					start_graph(ctx);
					printf("  \"" PKG_VER_FMT "\" -> \"" PKG_VER_FMT "\"%s;\n",
						PKG_VER_PRINTF(pkg),
						PKG_VER_PRINTF(pkg0),
						r ? "[color=red]" : "");
				}
			}
		}
	}
	ret -= S_EVALUATING - pkg->size;
	pkg->size = S_EVALUATED;

	return ret;
}

static int foreach_pkg(apk_hash_item item, void *ctx)
{
	dump_pkg((struct dot_ctx *) ctx, (struct apk_package *) item);
	return 0;
}

static int dot_main(void *pctx, struct apk_database *db, int argc, char **argv)
{
	struct dot_ctx *ctx = (struct dot_ctx *) pctx;
	int i, j;

	if (argc) {
		for (i = 0; i < argc; i++) {
			struct apk_name *name = apk_db_get_name(db, APK_BLOB_STR(argv[i]));
			if (!name)
				continue;
			for (j = 0; j < name->pkgs->num; j++)
				dump_pkg(ctx, name->pkgs->item[j]);
		}
	} else {
		apk_hash_foreach(&db->available.packages, foreach_pkg, pctx);
	}

	if (!ctx->not_empty)
		return 1;

	printf("}\n");
	return 0;
}

static struct apk_option dot_options[] = {
	{ 0x10000,	"errors",	"Output only parts of the graph which are considered "
					"errorneus: e.g. cycles and missing packages" },
};

static struct apk_applet apk_dot = {
	.name = "dot",
	.help = "Generate graphviz graphs",
	.arguments = "PKGMASK...",
	.open_flags = APK_OPENF_READ | APK_OPENF_NO_STATE,
	.context_size = sizeof(struct dot_ctx),
	.num_options = ARRAY_SIZE(dot_options),
	.options = dot_options,
	.parse = dot_parse,
	.main = dot_main,
};

APK_DEFINE_APPLET(apk_dot);
