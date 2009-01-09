/* info.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2009 Natanael Copa <n@tanael.org>
 * Copyright (C) 2009 Timo Teräs <timo.teras@iki.fi>
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

static int info_main(int argc, char **argv)
{
	struct apk_database db;
	struct apk_package *pkg;

	if (apk_db_open(&db, apk_root) < 0)
		return -1;

	list_for_each_entry(pkg, &db.installed.packages, installed_pkgs_list) {
		printf("%s", pkg->name->name);
		if (!apk_quiet) 
			printf(" %s", pkg->version);
		printf("\n");
	}

	apk_db_close(&db);
	return 0;
}

static struct apk_applet apk_info = {
	.name = "info",
	.usage = "",
	.main = info_main,
};

APK_DEFINE_APPLET(apk_info);

