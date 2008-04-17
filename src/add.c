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

#include <stdio.h>
#include "apk_applet.h"
#include "apk_database.h"

static int add_main(int argc, char **argv)
{
	struct apk_database db;
	int i;

	apk_db_init(&db, "/home/fabled/tmproot/");
	apk_db_read_config(&db);
	for (i = 0; i < argc; i++) {
		struct apk_dependency dep = {
			.name = apk_db_get_name(&db, argv[i]),
		};
		apk_deps_add(&db.world, &dep);
	}
	apk_db_recalculate_and_commit(&db);
	apk_db_free(&db);

	return 0;
}

static struct apk_applet apk_add = {
	.name = "add",
	.usage = "apkname...",
	.main = add_main,
};

APK_DEFINE_APPLET(apk_add);

