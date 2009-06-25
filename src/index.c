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
	int unsatisfied;
};

struct index_ctx {
	const char *index_file;
	int delete;
};

static int index_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct index_ctx *ictx = (struct index_ctx *) ctx;

	switch (optch) {
	case 'd':
		ictx->index_file = optarg;
		ictx->delete = 1;
		break;
	default:
		return -1;
	}
	return 0;
}

static int index_read_file(struct apk_database *db, struct index_ctx *ictx)
{
	struct apk_istream *is;
	int r;
	if (ictx->index_file == NULL)
		return 0;
	is = apk_bstream_gunzip(apk_bstream_from_url(ictx->index_file), 1);
	if (is == NULL)
		return -1;
	r = apk_db_index_read(db, is, -1);
	is->close(is);
	return r;
}

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

	return 0;
}

static int index_main(void *ctx, int argc, char **argv)
{
	struct apk_database db;
	struct counts counts = {0};
	struct apk_ostream *os;
	int total, i, j;
	struct index_ctx *ictx = (struct index_ctx *) ctx;

	apk_db_open(&db, NULL, APK_OPENF_READ);
	index_read_file(&db, ictx);

	for (i = 0; i < argc; i++) {
		if (ictx->delete) {
			struct apk_name *name;
			name = apk_db_query_name(&db, APK_BLOB_STR(argv[i]));
			if (name == NULL || name->pkgs == NULL)
				continue;
			/* apk_db_index_write() will only print the pkgs
			   where repos == 0. We prevent to write the given
			   packages by setting repos to non-zero */
			for (j = 0; j < name->pkgs->num; j++)
				name->pkgs->item[j]->repos = -1;
		} else
			apk_db_pkg_add_file(&db, argv[i]);
	}

	os = apk_ostream_to_fd(STDOUT_FILENO);
	total = apk_db_index_write(&db, os);
	os->close(os);

	apk_hash_foreach(&db.available.names, warn_if_no_providers, &counts);
	apk_db_close(&db);

	if (counts.unsatisfied != 0)
		apk_warning("Total of %d unsatisfiable package "
			    "names. Your repository maybe broken.",
			    counts.unsatisfied);
	apk_message("Index has %d packages", total);

	return 0;
}

static struct apk_option index_options[] = {
	{ 'd', "delete",
	  "Read existing INDEXFILE and delete the listed FILEs from it",
	  required_argument, "INDEXFILE" },
};

static struct apk_applet apk_index = {
	.name = "index",
	.help = "Create repository index file from FILEs.",
	.arguments = "FILE...",
	.context_size = sizeof(struct index_ctx),
	.num_options = ARRAY_SIZE(index_options),
	.options = index_options,
	.parse = index_parse,
	.main = index_main,
};

APK_DEFINE_APPLET(apk_index);

