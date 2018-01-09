/* dot.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
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
	int not_empty : 1;
	int errors_only : 1;
	int installed_only : 1;
};

static int option_parse_applet(void *pctx, struct apk_db_options *dbopts, int optch, const char *optarg)
{
	struct dot_ctx *ctx = (struct dot_ctx *) pctx;

	switch (optch) {
	case 0x10000:
		ctx->errors_only = 1;
		break;
	case 0x10001:
		ctx->installed_only = 1;
		dbopts->open_flags &= ~APK_OPENF_NO_INSTALLED;
		break;
	default:
		return -ENOTSUP;
	}
	return 0;
}

static const struct apk_option options_applet[] = {
	{ 0x10000,	"errors",	"Output only parts of the graph which are considered "
					"erroneous: e.g. cycles and missing packages" },
	{ 0x10001,	"installed",	"Consider only installed packages" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "Dot",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

static void start_graph(struct dot_ctx *ctx)
{
	if (ctx->not_empty)
		return;
	ctx->not_empty = 1;

	printf( "digraph \"apkindex\" {\n"
		"  rankdir=LR;\n"
		"  node [shape=box];\n");
}

static void dump_name(struct dot_ctx *ctx, struct apk_name *name)
{
	if (name->state_int)
		return;
	name->state_int = 1;

	if (name->providers->num == 0) {
		start_graph(ctx);
		printf("  \"%s\" [style=dashed, color=red, fontcolor=red, shape=octagon];\n",
			name->name);
	}
}

static int dump_pkg(struct dot_ctx *ctx, struct apk_package *pkg)
{
	struct apk_dependency *dep;
	struct apk_provider *p0;
	int r, ret = 0;

	if (ctx->installed_only && pkg->ipkg == NULL)
		return 0;

	if (pkg->state_int == S_EVALUATED)
		return 0;

	if (pkg->state_int <= S_EVALUATING) {
		pkg->state_int--;
		return 1;
	}

	pkg->state_int = S_EVALUATING;
	foreach_array_item(dep, pkg->depends) {
		struct apk_name *name = dep->name;

		dump_name(ctx, name);

		if (name->providers->num == 0) {
			printf("  \"" PKG_VER_FMT "\" -> \"%s\" [color=red];\n",
				PKG_VER_PRINTF(pkg), name->name);
			continue;
		}

		foreach_array_item(p0, name->providers) {
			if (ctx->installed_only && p0->pkg->ipkg == NULL)
				continue;
			if (!apk_dep_is_provided(dep, p0))
				continue;

			r = dump_pkg(ctx, p0->pkg);
			ret += r;
			if (r || (!ctx->errors_only)) {
				start_graph(ctx);

				printf("  \"" PKG_VER_FMT "\" -> \"" PKG_VER_FMT "\"[",
					PKG_VER_PRINTF(pkg),
					PKG_VER_PRINTF(p0->pkg));
				if (r)
					printf("color=red,");
				if (p0->pkg->name != dep->name)
					printf("arrowhead=inv,label=\"%s\",", dep->name->name);
				printf("];\n");
			}
		}
	}
	ret -= S_EVALUATING - pkg->state_int;
	pkg->state_int = S_EVALUATED;

	return ret;
}

static int foreach_pkg(apk_hash_item item, void *ctx)
{
	dump_pkg((struct dot_ctx *) ctx, (struct apk_package *) item);
	return 0;
}

static int dot_main(void *pctx, struct apk_database *db, struct apk_string_array *args)
{
	struct dot_ctx *ctx = (struct dot_ctx *) pctx;
	struct apk_provider *p;
	char **parg;

	if (args->num) {
		foreach_array_item(parg, args) {
			struct apk_name *name = apk_db_get_name(db, APK_BLOB_STR(*parg));
			if (!name)
				continue;
			foreach_array_item(p, name->providers)
				dump_pkg(ctx, p->pkg);
		}
	} else {
		apk_hash_foreach(&db->available.packages, foreach_pkg, pctx);
	}

	if (!ctx->not_empty)
		return 1;

	printf("}\n");
	return 0;
}

static struct apk_applet apk_dot = {
	.name = "dot",
	.help = "Generate graphviz graphs",
	.arguments = "PKGMASK...",
	.open_flags = APK_OPENF_READ | APK_OPENF_NO_STATE,
	.command_groups = APK_COMMAND_GROUP_QUERY,
	.context_size = sizeof(struct dot_ctx),
	.optgroups = { &optgroup_global, &optgroup_applet },
	.main = dot_main,
};

APK_DEFINE_APPLET(apk_dot);
