/* del.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include "apk_applet.h"
#include "apk_state.h"
#include "apk_database.h"

static int del_main(void *ctx, int argc, char **argv)
{
	struct apk_database db;
	struct apk_state *state;
	struct apk_name *name;
	int i, j, r;

	if (apk_db_open(&db, apk_root, APK_OPENF_WRITE) < 0)
		return -1;

	if (db.world == NULL)
		goto out;

	state = apk_state_new(&db);
	for (i = 0; i < argc; i++) {
		struct apk_dependency dep;

		name = apk_db_get_name(&db, APK_BLOB_STR(argv[i]));

		/* Remove from world, so we get proper changeset */
		for (j = 0; j < db.world->num; j++) {
			if (strcmp(db.world->item[j].name->name,
				   argv[i]) == 0) {
				db.world->item[j] =
					db.world->item[db.world->num-1];
				db.world =
					apk_dependency_array_resize(db.world, db.world->num-1);
			}
		}
		name->flags &= ~APK_NAME_TOPLEVEL;

		dep = (struct apk_dependency) {
			.name = name,
			.result_mask = APK_DEPMASK_CONFLICT,
		};

		r = apk_state_lock_dependency(state, &dep);
		if (r != 0) {
			apk_error("Unable to remove '%s'", name->name);
			goto err;
		}
	}
	r = apk_state_commit(state, &db);
err:
	apk_state_unref(state);
out:
	apk_db_close(&db);

	return r;
}

static struct apk_applet apk_del = {
	.name = "del",
	.usage = "apkname...",
	.main = del_main,
};

APK_DEFINE_APPLET(apk_del);

