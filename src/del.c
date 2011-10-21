/* del.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_print.h"
#include "apk_solver.h"

struct del_ctx {
	int recursive_delete : 1;
	struct apk_dependency_array *world;
};

static int del_parse(void *pctx, struct apk_db_options *db,
		     int optch, int optindex, const char *optarg)
{
	struct del_ctx *ctx = (struct del_ctx *) pctx;

	switch (optch) {
	case 'r':
		ctx->recursive_delete = 1;
		break;
	default:
		return -1;
	}
	return 0;
}

static void foreach_installed_reverse_dependency(
	struct apk_package *pkg,
	void (*cb)(struct apk_package *rdepend, void *ctx), void *ctx)
{
	struct apk_name *name;
	int i, j, k;

	if (pkg == NULL)
		return;

	name = pkg->name;
	for (i = 0; i < pkg->name->rdepends->num; i++) {
		struct apk_name *name0 = pkg->name->rdepends->item[i];

		for (j = 0; j < name0->pkgs->num; j++) {
			struct apk_package *pkg0 = name0->pkgs->item[j];

			if (pkg0->ipkg == NULL)
				continue;

			for (k = 0; k < pkg0->depends->num; k++) {
				struct apk_dependency *dep = &pkg0->depends->item[k];
				if (dep->name == name &&
				    apk_dep_is_satisfied(dep, pkg))
					break;
			}
			if (k >= pkg0->depends->num)
				continue;

			cb(pkg0, ctx);
		}
	}
}

struct not_deleted_ctx {
	struct apk_indent indent;
	struct apk_package *pkg;
	int header;
};

static void print_not_deleted_message(struct apk_package *pkg,
				      void *pctx)
{
	struct not_deleted_ctx *ctx = (struct not_deleted_ctx *) pctx;

	if (pkg == NULL)
		return;

	if (pkg->state_ptr == ctx->pkg)
		return;
	pkg->state_ptr = ctx->pkg;

	if (!ctx->header) {
		apk_message("World updated, but the following packages are not removed due to:");
		ctx->header = 1;
	}
	if (!ctx->indent.indent) {
		ctx->indent.x = printf("  %s:", ctx->pkg->name->name);
		ctx->indent.indent = ctx->indent.x + 1;
	}

	apk_print_indented(&ctx->indent, APK_BLOB_STR(pkg->name->name));
	foreach_installed_reverse_dependency(pkg, print_not_deleted_message, pctx);
}

static struct apk_package *name_to_pkg(struct apk_name *name)
{
	int i;

	for (i = 0; i < name->pkgs->num; i++) {
		if (name->pkgs->item[i]->ipkg != NULL)
			return name->pkgs->item[i];
	}

	return NULL;
}

static void delete_from_world(struct apk_package *pkg, void *pctx)
{
	struct del_ctx *ctx = (struct del_ctx *) pctx;

	apk_deps_del(&ctx->world, pkg->name);
	foreach_installed_reverse_dependency(pkg, delete_from_world, pctx);
}

static int del_main(void *pctx, struct apk_database *db, int argc, char **argv)
{
	struct del_ctx *ctx = (struct del_ctx *) pctx;
	struct apk_name **name;
	struct apk_changeset changeset = {};
	struct apk_package_array *solution = NULL;
	struct not_deleted_ctx ndctx = {};
	int i, r = 0;

	apk_dependency_array_copy(&ctx->world, db->world);

	name = alloca(argc * sizeof(struct apk_name*));
	for (i = 0; i < argc; i++) {
		name[i] = apk_db_get_name(db, APK_BLOB_STR(argv[i]));
		apk_deps_del(&ctx->world, name[i]);
		if (ctx->recursive_delete)
			foreach_installed_reverse_dependency(
					name_to_pkg(name[i]),
					delete_from_world, ctx);
	}

	r = apk_solver_solve(db, 0, ctx->world, &solution, &changeset);
	if (r == 0 || (apk_flags & APK_FORCE)) {
		/* check for non-deleted package names */
		for (i = 0; i < solution->num; i++) {
			struct apk_package *pkg = solution->item[i];
			pkg->name->state_ptr = pkg;
			pkg->state_int = 0;
		}
		for (i = 0; i < argc; i++) {
			ndctx.pkg = name[i]->state_ptr;
			ndctx.indent.indent = 0;
			foreach_installed_reverse_dependency(
					name[i]->state_ptr,
					print_not_deleted_message, &ndctx);
			if (ndctx.indent.indent)
				printf("\n");
		}
		if (ndctx.header)
			printf("\n");
		apk_solver_commit_changeset(db, &changeset, ctx->world);
		r = 0;
	} else {
		apk_solver_print_errors(db, solution, ctx->world, r);
	}
	apk_package_array_free(&solution);
	apk_dependency_array_free(&ctx->world);

	return r;
}

static struct apk_option del_options[] = {
	{ 'r', "rdepends",	"Recursively delete all top-level reverse "
				"dependencies too." },
};

static struct apk_applet apk_del = {
	.name = "del",
	.help = "Remove PACKAGEs from the main dependencies and uninstall them.",
	.arguments = "PACKAGE...",
	.open_flags = APK_OPENF_WRITE,
	.context_size = sizeof(struct del_ctx),
	.num_options = ARRAY_SIZE(del_options),
	.options = del_options,
	.parse = del_parse,
	.main = del_main,
};

APK_DEFINE_APPLET(apk_del);

