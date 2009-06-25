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

static int upgrade_main(void *ctx, int argc, char **argv)
{
	struct apk_database db;
	struct apk_state *state = NULL;
	int i, r;

	apk_flags |= APK_UPGRADE;

	r = apk_db_open(&db, apk_root, APK_OPENF_WRITE);
	if (r != 0)
		return r;

	state = apk_state_new(&db);
	for (i = 0; i < db.world->num; i++) {
		r = apk_state_lock_dependency(state, &db.world->item[i]);
		if (r != 0) {
			apk_error("Unable to upgrade '%s'",
				  db.world->item[i].name->name);
			goto err;
		}
	}
	r = apk_state_commit(state, &db);
err:
	if (state != NULL)
		apk_state_unref(state);
	apk_db_close(&db);
	return r;
}

static struct apk_applet apk_upgrade = {
	.name = "upgrade",
	.usage = "",
	.main = upgrade_main,
};

APK_DEFINE_APPLET(apk_upgrade);

