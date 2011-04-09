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
#include "apk_state.h"
#include "apk_print.h"

static int upgrade_parse(void *ctx, struct apk_db_options *dbopts,
			 int optch, int optindex, const char *optarg)
{
	switch (optch) {
	case 'a':
		apk_flags |= APK_PREFER_AVAILABLE;
		break;
	default:
		return -1;
	}
	return 0;
}

int apk_do_self_upgrade(struct apk_database *db, struct apk_state *state)
{
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
}

static int upgrade_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct apk_state *state = NULL;
	struct apk_name_array *missing;
	int i, r = 0;

	apk_flags |= APK_UPGRADE;
	apk_name_array_init(&missing);

	state = apk_state_new(db);
	if (state == NULL)
		goto err;

	r = apk_do_self_upgrade(db, state);
	if (r != 0) {
		apk_state_print_errors(state);
		goto err;
	}

	for (i = 0; i < db->world->num; i++) {
		struct apk_dependency *dep = &db->world->item[i];

		if (dep->name->pkgs->num != 0)
			r |= apk_state_lock_dependency(state, dep);
		else
			*apk_name_array_add(&missing) = dep->name;
	}
	if (r == 0) {
		for (i = 0; i < missing->num; i++) {
			struct apk_indent indent;
			struct apk_name *name = missing->item[i];

			if (i == 0) {
				apk_warning("The following package names no longer exists in repository:");
				indent.x = 0;
				indent.indent = 2;
			}
			apk_print_indented(&indent, APK_BLOB_STR(name->name));
		}
		if (i != 0)
			printf("\n");

		r = apk_state_commit(state);
	} else
		apk_state_print_errors(state);
err:
	if (state != NULL)
		apk_state_unref(state);
	apk_name_array_free(&missing);
	return r;
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

