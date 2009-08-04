/* add.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
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
#include "apk_state.h"

struct add_ctx {
	unsigned int open_flags;
	const char *virtpkg;
};

static int add_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct add_ctx *actx = (struct add_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		actx->open_flags |= APK_OPENF_CREATE;
		break;
	case 'u':
		apk_flags |= APK_UPGRADE;
		break;
	case 't':
		actx->virtpkg = optarg;
		break;
	default:
		return -1;
	}
	return 0;
}

static int non_repository_check(struct apk_database *db)
{
	if (apk_flags & APK_FORCE)
		return 0;
	if (apk_db_cache_active(db))
		return 0;
	if (apk_db_permanent(db))
		return 0;

	apk_error("You tried to add a non-repository package to system, "
		  "but it would be lost on next reboot. Enable package caching "
		  "(apk cache --help) or use --force if you know what you are "
		  "doing.");
	return 1;
}


static int add_main(void *ctx, int argc, char **argv)
{
	struct add_ctx *actx = (struct add_ctx *) ctx;
	struct apk_database db;
	struct apk_state *state = NULL;
	struct apk_dependency_array *pkgs = NULL;
	struct apk_package *virtpkg = NULL;
	struct apk_dependency virtdep;
	int i, r;

	r = apk_db_open(&db, apk_root, actx->open_flags | APK_OPENF_WRITE);
	if (r != 0)
		return r;

	if (actx->virtpkg) {
		if (non_repository_check(&db))
			goto err;

		virtpkg = apk_pkg_new();
		if (virtpkg == NULL) {
			apk_error("Failed to allocate virtual meta package");
			goto err;
		}
		virtpkg->name = apk_db_get_name(&db, APK_BLOB_STR(actx->virtpkg));
		apk_blob_checksum(APK_BLOB_STR(virtpkg->name->name),
				  apk_default_checksum(), &virtpkg->csum);
		virtpkg->version = strdup("0");
		virtpkg->description = strdup("virtual meta package");
		apk_dep_from_pkg(&virtdep, &db, virtpkg);
		virtdep.name->flags |= APK_NAME_TOPLEVEL;
		virtpkg = apk_db_pkg_add(&db, virtpkg);
	}

	for (i = 0; i < argc; i++) {
		struct apk_dependency dep;

		if (strstr(argv[i], ".apk") != NULL) {
			struct apk_package *pkg = NULL;
			struct apk_sign_ctx sctx;

			if (non_repository_check(&db))
				goto err;

			apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY_AND_GENERATE,
					  NULL, db.keys_fd);
			r = apk_pkg_read(&db, argv[i], &sctx, &pkg);
			apk_sign_ctx_free(&sctx);
			if (r != 0) {
				apk_error("%s: %s", argv[i], apk_error_str(r));
				goto err;

			}
			apk_dep_from_pkg(&dep, &db, pkg);
		} else {
			r = apk_dep_from_blob(&dep, &db, APK_BLOB_STR(argv[i]));
			if (r != 0)
				goto err;
		}

		if (virtpkg) {
			apk_deps_add(&virtpkg->depends, &dep);
		} else {
			dep.name->flags |= APK_NAME_TOPLEVEL;
		}
		apk_deps_add(&pkgs, &dep);
	}

	if (virtpkg) {
		apk_deps_add(&pkgs, &virtdep);
		apk_deps_add(&db.world, &virtdep);
	}

	state = apk_state_new(&db);
	if (state == NULL)
		goto err;

	for (i = 0; (pkgs != NULL) && i < pkgs->num; i++) {
		r = apk_state_lock_dependency(state, &pkgs->item[i]);
		if (r != 0) {
			apk_error("Unable to install '%s'", pkgs->item[i].name->name);
			if (!(apk_flags & APK_FORCE))
				goto err;
		}
		if (!virtpkg)
			apk_deps_add(&db.world, &pkgs->item[i]);
	}
	r = apk_state_commit(state, &db);
err:
	if (state != NULL)
		apk_state_unref(state);
	apk_db_close(&db);
	return r;
}

static struct apk_option add_options[] = {
	{ 0x10000,	"initdb",	"Initialize database" },
	{ 'u',		"upgrade",	"Prefer to upgrade package" },
	{ 't',		"virtual",
	  "Instead of adding all the packages to 'world', create a new virtual "
	  "package with the listed dependencies and add that to 'world'. The "
	  "actions of the command are easily reverted by deleting the virtual "
	  "package.", required_argument, "NAME" },
};

static struct apk_applet apk_add = {
	.name = "add",
	.help = "Add (or update) PACKAGEs to main dependencies and install "
		"them, while ensuring that all dependencies are met.",
	.arguments = "PACKAGE...",
	.context_size = sizeof(struct add_ctx),
	.num_options = ARRAY_SIZE(add_options),
	.options = add_options,
	.parse = add_parse,
	.main = add_main,
};

APK_DEFINE_APPLET(apk_add);

