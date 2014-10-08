/* upgrade.c - Alpine Package Keeper (APK)
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
#include <stdlib.h>
#include <unistd.h>
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_print.h"
#include "apk_solver.h"

struct upgrade_ctx {
	unsigned short solver_flags;
	int no_self_upgrade : 1;
};

static int option_parse_applet(void *ctx, struct apk_db_options *dbopts, int optch, const char *optarg)
{
	struct upgrade_ctx *uctx = (struct upgrade_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		uctx->no_self_upgrade = 1;
		break;
	case 'a':
		uctx->solver_flags |= APK_SOLVERF_AVAILABLE;
		break;
	case 'l':
		uctx->solver_flags |= APK_SOLVERF_LATEST;
		break;
	default:
		return -1;
	}
	return 0;
}

static const struct apk_option options_applet[] = {
	{ 'a', "available",
	  "Resets versioned world dependencies, and changes to prefer "
	  "replacing or downgrading packages (instead of holding them) "
	  "if the currently installed package is no longer available "
	  "from any repository" },
	{ 'l', "latest",
	  "Select latest version of package (if it is not pinned), and "
	  "print error if it cannot be installed due to other dependencies" },
	{ 0x10000, "no-self-upgrade",
	  "Do not do early upgrade of 'apk-tools' package" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "Upgrade",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

int apk_do_self_upgrade(struct apk_database *db, unsigned short solver_flags)
{
	struct apk_name *name;
	struct apk_changeset changeset = {};
	int r;

	name = apk_db_get_name(db, APK_BLOB_STR("apk-tools"));
	apk_solver_set_name_flags(name, solver_flags, 0);
	db->performing_self_update = 1;

	r = apk_solver_solve(db, 0, db->world, &changeset);
	if (r != 0) {
		apk_solver_print_errors(db, &changeset, db->world);
		goto ret;
	}

	if (changeset.num_total_changes == 0)
		goto ret;

	if (apk_flags & APK_SIMULATE) {
		apk_warning("This simulation is not reliable as apk-tools upgrade is available.");
		goto ret;
	}

	apk_message("Upgrading critical system libraries and apk-tools:");
	apk_solver_commit_changeset(db, &changeset, db->world);
	apk_db_close(db);

	apk_message("Continuing the upgrade transaction with new apk-tools:");
	for (r = 0; apk_argv[r] != NULL; r++)
		;
	apk_argv[r] = "--no-self-upgrade";
	execvp(apk_argv[0], apk_argv);

	apk_error("PANIC! Failed to re-execute new apk-tools!");
	exit(1);

ret:
	apk_change_array_free(&changeset.changes);
	db->performing_self_update = 0;

	return r;
}

static int upgrade_main(void *ctx, struct apk_database *db, struct apk_string_array *args)
{
	struct upgrade_ctx *uctx = (struct upgrade_ctx *) ctx;
	unsigned short solver_flags;
	struct apk_dependency *dep;
	struct apk_dependency_array *world = NULL;
	int r;

	if (apk_db_check_world(db, db->world) != 0) {
		apk_error("Not continuing with upgrade due to missing repository tags. Use --force to override.");
		return -1;
	}

	solver_flags = APK_SOLVERF_UPGRADE | uctx->solver_flags;
	if (!uctx->no_self_upgrade) {
		r = apk_do_self_upgrade(db, solver_flags);
		if (r != 0)
			return r;
	}

	if (solver_flags & APK_SOLVERF_AVAILABLE) {
		apk_dependency_array_copy(&world, db->world);
		foreach_array_item(dep, world) {
			if (dep->result_mask == APK_DEPMASK_CHECKSUM) {
				dep->result_mask = APK_DEPMASK_ANY;
				dep->version = apk_blob_atomize(APK_BLOB_NULL);
			}
		}
	} else {
		world = db->world;
	}

	r = apk_solver_commit(db, solver_flags, world);

	if (solver_flags & APK_SOLVERF_AVAILABLE)
		apk_dependency_array_free(&world);

	return r;
}

static struct apk_applet apk_upgrade = {
	.name = "upgrade",
	.help = "Upgrade currently installed packages to match repositories",
	.open_flags = APK_OPENF_WRITE,
	.context_size = sizeof(struct upgrade_ctx),
	.optgroups = { &optgroup_global, &optgroup_commit, &optgroup_applet },
	.main = upgrade_main,
};

APK_DEFINE_APPLET(apk_upgrade);

