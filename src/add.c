/* add.c - Alpine Package Keeper (APK)
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

struct add_ctx {
	const char *virtpkg;
	unsigned short solver_flags;
};

static int option_parse_applet(void *ctx, struct apk_db_options *dbopts, int optch, const char *optarg)
{
	struct add_ctx *actx = (struct add_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		dbopts->open_flags |= APK_OPENF_CREATE;
		break;
	case 'u':
		actx->solver_flags |= APK_SOLVERF_UPGRADE;
		break;
	case 'l':
		actx->solver_flags |= APK_SOLVERF_LATEST;
		break;
	case 't':
		actx->virtpkg = optarg;
		break;
	default:
		return -ENOTSUP;
	}
	return 0;
}

static const struct apk_option options_applet[] = {
	{ 0x10000,	"initdb",	"Initialize database" },
	{ 'u',		"upgrade",	"Prefer to upgrade package" },
        { 'l',		"latest",
	  "Select latest version of package (if it is not pinned), and "
	  "print error if it cannot be installed due to other dependencies" },
	{ 't',		"virtual",
	  "Instead of adding all the packages to 'world', create a new virtual "
	  "package with the listed dependencies and add that to 'world'; the "
	  "actions of the command are easily reverted by deleting the virtual "
	  "package", required_argument, "NAME" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "Add",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

static int non_repository_check(struct apk_database *db)
{
	if (apk_force & APK_FORCE_NON_REPOSITORY)
		return 0;
	if (apk_db_cache_active(db))
		return 0;
	if (apk_db_permanent(db))
		return 0;

	apk_error("You tried to add a non-repository package to system, "
		  "but it would be lost on next reboot. Enable package caching "
		  "(apk cache --help) or use --force-non-repository "
		  "if you know what you are doing.");
	return 1;
}

static int add_main(void *ctx, struct apk_database *db, struct apk_string_array *args)
{
	struct add_ctx *actx = (struct add_ctx *) ctx;
	struct apk_package *virtpkg = NULL;
	struct apk_dependency virtdep;
	struct apk_dependency_array *world = NULL;
	char **parg;
	int r = 0;

	apk_dependency_array_copy(&world, db->world);

	if (actx->virtpkg) {
		apk_blob_t b = APK_BLOB_STR(actx->virtpkg);

		apk_blob_pull_dep(&b, db, &virtdep);
		if (APK_BLOB_IS_NULL(b) || virtdep.conflict ||
		    virtdep.result_mask != APK_DEPMASK_ANY ||
		    virtdep.version != &apk_null_blob) {
			apk_error("%s: bad package specifier");
			return -1;
		}

		if (virtdep.name->name[0] != '.' && non_repository_check(db))
			return -1;

		virtpkg = apk_pkg_new();
		if (virtpkg == NULL) {
			apk_error("Failed to allocate virtual meta package");
			return -1;
		}
		virtpkg->name = virtdep.name;
		apk_blob_checksum(APK_BLOB_STR(virtpkg->name->name),
				  apk_checksum_default(), &virtpkg->csum);
		virtpkg->version = apk_blob_atomize(APK_BLOB_STR("0"));
		virtpkg->description = strdup("virtual meta package");
		virtpkg->arch = apk_blob_atomize(APK_BLOB_STR("noarch"));
	}

	foreach_array_item(parg, args) {
		struct apk_dependency dep;

		if (strstr(*parg, ".apk") != NULL) {
			struct apk_package *pkg = NULL;
			struct apk_sign_ctx sctx;

			if (non_repository_check(db))
				return -1;

			apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY_AND_GENERATE,
					  NULL, db->keys_fd);
			r = apk_pkg_read(db, *parg, &sctx, &pkg);
			apk_sign_ctx_free(&sctx);
			if (r != 0) {
				apk_error("%s: %s", *parg, apk_error_str(r));
				return -1;
			}
			apk_dep_from_pkg(&dep, db, pkg);
		} else {
			apk_blob_t b = APK_BLOB_STR(*parg);

			apk_blob_pull_dep(&b, db, &dep);
			if (APK_BLOB_IS_NULL(b) || b.len > 0 || (virtpkg != NULL && dep.repository_tag)) {
				apk_error("'%s' is not a valid %s dependency, format is %s",
					  *parg, virtpkg == NULL ? "world" : "child",
					  virtpkg == NULL ? "name(@tag)([<>~=]version)" : "name([<>~=]version)");
				return -1;
			}
		}

		if (virtpkg == NULL) {
			apk_deps_add(&world, &dep);
			apk_solver_set_name_flags(dep.name,
						  actx->solver_flags,
						  actx->solver_flags);
		} else {
			apk_deps_add(&virtpkg->depends, &dep);
		}
	}
	if (virtpkg) {
		virtpkg = apk_db_pkg_add(db, virtpkg);
		apk_deps_add(&world, &virtdep);
		apk_solver_set_name_flags(virtdep.name,
					  actx->solver_flags,
					  actx->solver_flags);
	}

	r = apk_solver_commit(db, 0, world);
	apk_dependency_array_free(&world);

	return r;
}

static struct apk_applet apk_add = {
	.name = "add",
	.help = "Add PACKAGEs to 'world' and install (or upgrade) "
		"them, while ensuring that all dependencies are met",
	.arguments = "PACKAGE...",
	.open_flags = APK_OPENF_WRITE,
	.command_groups = APK_COMMAND_GROUP_INSTALL,
	.context_size = sizeof(struct add_ctx),
	.optgroups = { &optgroup_global, &optgroup_commit, &optgroup_applet },
	.main = add_main,
};

APK_DEFINE_APPLET(apk_add);
