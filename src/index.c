/* index.c - Alpine Package Keeper (APK)
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
#include <unistd.h>

#include "apk_applet.h"
#include "apk_database.h"

struct counts {
	int total;
	int unsatisfied;
};

static int warn_if_no_providers(apk_hash_item item, void *ctx)
{
	struct counts *counts = (struct counts *) ctx;
	struct apk_name *name = (struct apk_name *) item;

	if (name->pkgs == NULL) {
		if (++counts->unsatisfied < 10) {
			apk_warning("No provider for dependency '%s'",
				    name->name);
		} else if (counts->unsatisfied == 10) {
			apk_warning("Too many unsatisfiable dependencies, "
				    "not reporting the rest.");
		}
	}
	counts->total++;

	return 0;
}

static int index_main(void *ctx, int argc, char **argv)
{
	struct apk_database db;
	struct counts counts = {0,0};
	struct apk_ostream *os;
	int i;

	apk_db_open(&db, NULL);
	for (i = 0; i < argc; i++)
		apk_db_pkg_add_file(&db, argv[i]);

	os = apk_ostream_to_fd(STDOUT_FILENO);
	apk_db_index_write(&db, os);
	os->close(os);

	apk_hash_foreach(&db.available.names, warn_if_no_providers, &counts);
	apk_db_close(&db);

	if (counts.unsatisfied != 0)
		apk_warning("Total of %d unsatisfiable package "
			    "names. Your repository maybe broken.",
			    counts.unsatisfied);
	apk_message("Index has %d packages", counts.total);

	return 0;
}

static struct apk_applet apk_index = {
	.name = "index",
	.usage = "apkname...",
	.main = index_main,
};

APK_DEFINE_APPLET(apk_index);

