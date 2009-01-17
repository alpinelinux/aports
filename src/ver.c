/* ver.c - Alpine Package Keeper (APK)
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

static int res2char(int res)
{
	switch (res) {
	case APK_VERSION_LESS:
		return '<';
	case APK_VERSION_GREATER:
		return '>';
	case APK_VERSION_EQUAL:
		return '=';
	default:
		return '?';
	}
}

static int ver_main(void *ctx, int argc, char **argv)
{
	struct apk_database db;
	struct apk_name *name;
	struct apk_package *pkg, *upg, *tmp;
	int i, r;

	if (argc == 2) {
		r = apk_version_compare(APK_BLOB_STR(argv[0]),
					APK_BLOB_STR(argv[1]));
		printf("%c\n", res2char(r));
		return 0;
	}

	if (apk_db_open(&db, apk_root, APK_OPENF_READ) < 0)
		return -1;

	list_for_each_entry(pkg, &db.installed.packages, installed_pkgs_list) {
		name = pkg->name;
		upg = pkg;
		for (i = 0; i < name->pkgs->num; i++) {
			tmp = name->pkgs->item[i];
			if (tmp->name != name)
				continue;
			r = apk_version_compare(APK_BLOB_STR(tmp->version),
						APK_BLOB_STR(upg->version));
			if (r == APK_VERSION_GREATER)
				upg = tmp;
		}
		printf("%-40s%c\n", name->name, pkg != upg ? '<' : '=');
	}
	apk_db_close(&db);

	return 0;
}

static struct apk_applet apk_ver = {
	.name = "version",
	.usage = "[version1 version2]",
	.main = ver_main,
};

APK_DEFINE_APPLET(apk_ver);

