/* upgrade.c - Alpine Package Keeper (APK)
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
#include <zlib.h>
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_print.h"
#include "apk_solver.h"

struct upgrade_ctx {
	unsigned short solver_flags;
};

static int upgrade_parse(void *ctx, struct apk_db_options *dbopts,
			 int optch, int optindex, const char *optarg)
{
	struct upgrade_ctx *uctx = (struct upgrade_ctx *) ctx;

	switch (optch) {
	case 'a':
		uctx->solver_flags |= APK_SOLVERF_AVAILABLE;
		break;
	default:
		return -1;
	}
	return 0;
}

int apk_do_self_upgrade(struct apk_database *db)
{
#if 0
	/* FIXME: Reimplement self-upgrade. */
	struct apk_dependency dep;
	int r;

	apk_dep_from_blob(&dep, db, APK_BLOB_STR("apk-tools"));

	r = apk_state_lock_dependency(state, &dep);
	if (r != 0 || state->num_changes == 0)
		return r;

	if (apk_flags & APK_SIMULATE) {
		apk_warning("This simulation is not reliable as apk-tools upgrade is available.");
		return 0;
	}

	apk_message("Upgrading critical system libraries and apk-tools:");
	state->print_ok = 0;
	r = apk_state_commit(state);
	apk_state_unref(state);
	apk_db_close(db);

	apk_message("Continuing the upgrade transaction with new apk-tools:");
	execvp(apk_argv[0], apk_argv);

	apk_error("PANIC! Failed to re-execute new apk-tools!");
	exit(1);
#else
	return 0;
#endif
}

static int upgrade_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct upgrade_ctx *uctx = (struct upgrade_ctx *) ctx;
	unsigned short solver_flags;
	int r;

	solver_flags = APK_SOLVERF_UPGRADE | uctx->solver_flags;

	r = apk_do_self_upgrade(db);
	if (r != 0)
		return r;

	return apk_solver_commit(db, solver_flags, db->world);
}

static struct apk_option upgrade_options[] = {
	{ 'a', "available",
	  "Re-install or downgrade if currently installed package is not "
	  "currently available from any repository" },
};

static struct apk_applet apk_upgrade = {
	.name = "upgrade",
	.help = "Upgrade (or downgrade with -a) the currently installed "
		"packages to versions available in repositories.",
	.open_flags = APK_OPENF_WRITE,
	.num_options = ARRAY_SIZE(upgrade_options),
	.options = upgrade_options,
	.parse = upgrade_parse,
	.main = upgrade_main,
};

APK_DEFINE_APPLET(apk_upgrade);

