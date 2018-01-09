/* fix.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <errno.h>
#include <stdio.h>
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_print.h"
#include "apk_solver.h"

struct fix_ctx {
	unsigned short solver_flags;
	int fix_depends : 1;
	int fix_xattrs : 1;
	int fix_directory_permissions : 1;
	int errors;
};

static int option_parse_applet(void *pctx, struct apk_db_options *dbopts, int optch, const char *optarg)
{
	struct fix_ctx *ctx = (struct fix_ctx *) pctx;
	switch (optch) {
	case 'd':
		ctx->fix_depends = 1;
		break;
	case 'x':
		ctx->fix_xattrs = 1;
		break;
	case 'u':
		ctx->solver_flags |= APK_SOLVERF_UPGRADE;
		break;
	case 'r':
		ctx->solver_flags |= APK_SOLVERF_REINSTALL;
		break;
	case 0x10000:
		ctx->fix_directory_permissions = 1;
		break;
	default:
		return -ENOTSUP;
	}
	return 0;
}

static const struct apk_option options_applet[] = {
	{ 'd',		"depends",	"Fix all dependencies too" },
	{ 'r',		"reinstall",	"Reinstall the package (default)" },
	{ 'u',		"upgrade",	"Prefer to upgrade package" },
	{ 'x',		"xattr",	"Fix packages with broken xattrs" },
	{ 0x10000,	"directory-permissions", "Reset all directory permissions" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "Fix",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

static int mark_recalculate(apk_hash_item item, void *ctx)
{
	struct apk_db_dir *dir = (struct apk_db_dir *) item;
	if (dir->refs == 0) return 0;
	dir->update_permissions = 1;
	return 0;
}

static void mark_fix(struct fix_ctx *ctx, struct apk_name *name)
{
	apk_solver_set_name_flags(name, ctx->solver_flags, ctx->fix_depends ? ctx->solver_flags : 0);
}

static void set_solver_flags(struct apk_database *db, const char *match, struct apk_name *name, void *pctx)
{
	struct fix_ctx *ctx = pctx;

	if (!name) {
		apk_error("Package '%s' not found", match);
		ctx->errors++;
	} else
		mark_fix(ctx, name);
}

static int fix_main(void *pctx, struct apk_database *db, struct apk_string_array *args)
{
	struct fix_ctx *ctx = (struct fix_ctx *) pctx;
	struct apk_installed_package *ipkg;

	if (!ctx->solver_flags)
		ctx->solver_flags = APK_SOLVERF_REINSTALL;

	if (ctx->fix_directory_permissions)
		apk_hash_foreach(&db->installed.dirs, mark_recalculate, db);

	if (args->num == 0) {
		list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
			if (ipkg->broken_files || ipkg->broken_script ||
			    (ipkg->broken_xattr && ctx->fix_xattrs))
				mark_fix(ctx, ipkg->pkg->name);
		}
	} else
		apk_name_foreach_matching(db, args, apk_foreach_genid(), set_solver_flags, ctx);

	if (ctx->errors) return ctx->errors;

	return apk_solver_commit(db, 0, db->world);
}

static struct apk_applet apk_fix = {
	.name = "fix",
	.help = "Repair package or upgrade it without modifying main "
		"dependencies",
	.arguments = "PACKAGE...",
	.open_flags = APK_OPENF_WRITE,
	.command_groups = APK_COMMAND_GROUP_SYSTEM,
	.context_size = sizeof(struct fix_ctx),
	.optgroups = { &optgroup_global, &optgroup_commit, &optgroup_applet },
	.main = fix_main,
};

APK_DEFINE_APPLET(apk_fix);

