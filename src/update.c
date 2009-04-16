/* update.c - Alpine Package Keeper (APK)
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
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_version.h"

static int update_main(void *ctx, int argc, char **argv)
{
	struct apk_database db;
	int i;

	if (apk_db_open(&db, apk_root, APK_OPENF_READ) < 0)
		return -1;

	for (i = 0; i < db.num_repos; i++)
		apk_repository_update(&db, &db.repos[i]);

	apk_db_close(&db);

	return 0;
}

static struct apk_applet apk_update = {
	.name = "update",
	.usage = "",
	.main = update_main,
};

APK_DEFINE_APPLET(apk_update);

