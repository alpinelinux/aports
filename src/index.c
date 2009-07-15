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
	const char *index;
	time_t index_mtime;
};

static int index_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct index_ctx *ictx = (struct index_ctx *) ctx;

	switch (optch) {
	case 'x':
		ictx->index = optarg;
		break;
	default:
		return -1;
	}
	return 0;
}

static int index_read_file(struct apk_database *db, struct index_ctx *ictx)
{
	struct apk_bstream *bs;
	struct apk_file_info fi;
	int r;
	if (ictx->index == NULL)
		return 0;
	if (apk_file_get_info(ictx->index, APK_CHECKSUM_NONE, &fi) < 0)
		return -1;
	ictx->index_mtime = fi.mtime;
	bs = apk_bstream_from_istream(apk_bstream_gunzip(apk_bstream_from_url(ictx->index), 1));
	if (bs == NULL)
		return -1;
	r = apk_db_index_read(db, bs, 0);
	bs->close(bs, NULL);
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
	struct apk_file_info fi;
	int total, i, j, found, newpkgs = 0;
	struct index_ctx *ictx = (struct index_ctx *) ctx;

	apk_db_open(&db, NULL, APK_OPENF_READ);
	if (index_read_file(&db, ictx) < 0) {
		apk_db_close(&db);
		apk_error("The index is corrupt, or of unknown format.");
		return -1;
	}

	for (i = 0; i < argc; i++) {
		if (apk_file_get_info(argv[i], APK_CHECKSUM_NONE, &fi) < 0) {
			apk_warning("File '%s' is unaccessible", argv[i]);
			continue;
		}

		found = FALSE;
		do {
			struct apk_name *name;
			char *fname, *fend;
			apk_blob_t bname, bver;

			/* Check if index is newer than package */
			if (ictx->index == NULL || ictx->index_mtime < fi.mtime)
				break;

			/* Check that it looks like a package name */
			fname = strrchr(argv[i], '/');
			if (fname == NULL)
				fname = argv[i];
			else
				fname++;
			fend = strstr(fname, ".apk");
			if (fend == NULL)
				break;
			if (apk_pkg_parse_name(APK_BLOB_PTR_PTR(fname, fend-1),
					       &bname, &bver) < 0)
				break;

			/* If we have it in the old index already? */
			name = apk_db_query_name(&db, bname);
			if (name == NULL || name->pkgs == NULL)
				break;

			for (j = 0; j < name->pkgs->num; j++) {
				struct apk_package *pkg = name->pkgs->item[j];
				if (apk_blob_compare(bver, APK_BLOB_STR(pkg->version)) != 0)
					continue;
				if (pkg->size != fi.size)
					continue;
				pkg->filename = strdup(argv[i]);
				found = TRUE;
				break;
			}
		} while (0);

		if (!found) {
			apk_db_pkg_add_file(&db, argv[i]);
			newpkgs++;
		}
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
	apk_message("Index has %d packages (of which %d are new)",
		    total, newpkgs);

	return 0;
}

static struct apk_option index_options[] = {
	{ 'x', "index", "Read INDEX to speed up new index creation by reusing "
	  "the information from an old index",
	  required_argument, "INDEX" },
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

