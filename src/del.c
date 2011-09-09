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
#include "apk_print.h"
#include "apk_solver.h"

static int del_parse(void *ctx, struct apk_db_options *db,
		     int optch, int optindex, const char *optarg)
{
	switch (optch) {
	case 'r':
		/* FIXME: Reimplement recursive delete. */
	default:
		return -1;
	}
	return 0;
}

static int del_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct apk_name *name;
	struct apk_dependency_array *world = NULL;
	int i, r = 0;

	apk_dependency_array_copy(&world, db->world);
	for (i = 0; i < argc; i++) {
		name = apk_db_get_name(db, APK_BLOB_STR(argv[i]));
		apk_deps_del(&world, name);
	}
	r = apk_solver_commit(db, 0, world);
	apk_dependency_array_free(&world);

	return r;
}

static struct apk_option del_options[] = {
	{ 'r', "rdepends",	"Recursively delete all top-level reverse "
				"dependencies too." },
};

static struct apk_applet apk_del = {
	.name = "del",
	.help = "Remove PACKAGEs from the main dependencies and uninstall them.",
	.arguments = "PACKAGE...",
	.open_flags = APK_OPENF_WRITE,
	.num_options = ARRAY_SIZE(del_options),
	.options = del_options,
	.parse = del_parse,
	.main = del_main,
};

APK_DEFINE_APPLET(apk_del);

