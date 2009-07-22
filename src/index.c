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
#include <fcntl.h>
#include <unistd.h>

#include "apk_applet.h"
#include "apk_database.h"

#define INDEX_OLD_FORMAT	0x10000

struct counts {
	int unsatisfied;
};

struct index_ctx {
	const char *index;
	const char *output;
	time_t index_mtime;
	int method;
};

static int index_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct index_ctx *ictx = (struct index_ctx *) ctx;

	switch (optch) {
	case 'x':
		ictx->index = optarg;
		break;
	case 'o':
		ictx->output = optarg;
		break;
	case INDEX_OLD_FORMAT:
		ictx->method = APK_SIGN_GENERATE_V1;
		break;
	default:
		return -1;
	}
	return 0;
}

static int index_read_file(struct apk_database *db, struct index_ctx *ictx)
{
	struct apk_file_info fi;

	if (ictx->index == NULL)
		return 0;
	if (apk_file_get_info(ictx->index, APK_CHECKSUM_NONE, &fi) < 0)
		return -1;
	ictx->index_mtime = fi.mtime;

	return apk_db_index_read_file(db, ictx->index, 0);
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

	if (isatty(STDOUT_FILENO) && ictx->output == NULL &&
	    !(apk_flags & APK_FORCE)) {
		apk_error("Will not write binary index to console "
			  "without --force");
		return -1;
	}

	if (ictx->method == 0)
		ictx->method = APK_SIGN_GENERATE;

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
			struct apk_sign_ctx sctx;
			apk_sign_ctx_init(&sctx, ictx->method, NULL);
			if (apk_pkg_read(&db, argv[i], &sctx, NULL) == 0)
				newpkgs++;
			apk_sign_ctx_free(&sctx);
		}
	}

	if (ictx->method == APK_SIGN_GENERATE) {
		memset(&fi, 0, sizeof(fi));
		fi.name = "APKINDEX";
		fi.mode = 0644 | S_IFREG;
		os = apk_ostream_counter(&fi.size);
		apk_db_index_write(&db, os);
		os->close(os);
	}

	if (ictx->output != NULL)
		os = apk_ostream_to_file(ictx->output, 0644);
	else
		os = apk_ostream_to_fd(STDOUT_FILENO);
	if (ictx->method == APK_SIGN_GENERATE) {
		os = apk_ostream_gzip(os);
		apk_tar_write_entry(os, &fi, NULL);
	}
	total = apk_db_index_write(&db, os);
	if (ictx->method == APK_SIGN_GENERATE) {
		apk_tar_write_padding(os, &fi);
		apk_tar_write_entry(os, NULL, NULL);
	}
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
	{ 'o', "output", "Write the generated index to FILE",
	  required_argument, "FILE" },
	{ 'x', "index", "Read INDEX to speed up new index creation by reusing "
	  "the information from an old index",
	  required_argument, "INDEX" },
	{ INDEX_OLD_FORMAT, "old-format",
	  "Specify to create old style index files" }
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

