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
#include "apk_database.h"

static int del_main(int argc, char **argv)
{
	struct apk_database db;
	int i, j;

	apk_db_init(&db, "/home/fabled/tmproot/");
	apk_db_read_config(&db);

	if (db.world == NULL)
		goto out;

	for (i = 0; i < argc; i++) {
		for (j = 0; j < db.world->num; j++) {
			if (strcmp(db.world->item[j].name->name,
				   argv[i]) == 0) {
				db.world->item[j] =
					db.world->item[db.world->num-1];
				db.world =
					apk_dependency_array_resize(db.world, db.world->num-1);
			}
		}
	}

	apk_db_recalculate_and_commit(&db);
out:
	apk_db_free(&db);

	return 0;
}

static struct apk_applet apk_del = {
	.name = "del",
	.usage = "apkname...",
	.main = del_main,
};

APK_DEFINE_APPLET(apk_del);

