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
#include "apk_print.h"

#define INDEX_OLD_FORMAT	0x10000

struct counts {
	int unsatisfied;
};

struct index_ctx {
	const char *index;
	const char *output;
	const char *description;
	time_t index_mtime;
	int method;
};

static int index_parse(void *ctx, struct apk_db_options *dbopts,
		       int optch, int optindex, const char *optarg)
{
	struct index_ctx *ictx = (struct index_ctx *) ctx;

	switch (optch) {
	case 'x':
		ictx->index = optarg;
		break;
	case 'o':
		ictx->output = optarg;
		break;
	case 'd':
		ictx->description = optarg;
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
	if (apk_file_get_info(AT_FDCWD, ictx->index, APK_CHECKSUM_NONE, &fi) < 0)
		return -1;
	ictx->index_mtime = fi.mtime;

	return apk_db_index_read_file(db, ictx->index, 0);
}

static int warn_if_no_providers(apk_hash_item item, void *ctx)
{
	struct counts *counts = (struct counts *) ctx;
	struct apk_name *name = (struct apk_name *) item;

	if (name->pkgs->num == 0) {
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

static int index_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct counts counts = {0};
	struct apk_ostream *os;
	struct apk_file_info fi;
	int total, r, i, j, found, newpkgs = 0;
	struct index_ctx *ictx = (struct index_ctx *) ctx;

	if (isatty(STDOUT_FILENO) && ictx->output == NULL &&
	    !(apk_flags & APK_FORCE)) {
		apk_error("Will not write binary index to console "
			  "without --force");
		return -1;
	}

	if (ictx->method == 0)
		ictx->method = APK_SIGN_GENERATE;

	if ((r = index_read_file(db, ictx)) < 0) {
		apk_error("%s: %s", ictx->index, apk_error_str(r));
		return r;
	}

	for (i = 0; i < argc; i++) {
		if (apk_file_get_info(AT_FDCWD, argv[i], APK_CHECKSUM_NONE, &fi) < 0) {
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
			name = apk_db_query_name(db, bname);
			if (name == NULL)
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
			apk_sign_ctx_init(&sctx, ictx->method, NULL, db->keys_fd);
			if (apk_pkg_read(db, argv[i], &sctx, NULL) == 0)
				newpkgs++;
			apk_sign_ctx_free(&sctx);
		}
	}

	if (ictx->output != NULL)
		os = apk_ostream_to_file(AT_FDCWD, ictx->output, NULL, 0644);
	else
		os = apk_ostream_to_fd(STDOUT_FILENO);

	if (ictx->method == APK_SIGN_GENERATE) {
		struct apk_ostream *counter;

		os = apk_ostream_gzip(os);

		if (ictx->description != NULL) {
			memset(&fi, 0, sizeof(fi));
			fi.mode = 0644 | S_IFREG;
			fi.name = "DESCRIPTION";
			fi.size = strlen(ictx->description);
			apk_tar_write_entry(os, &fi, ictx->description);
		}

		memset(&fi, 0, sizeof(fi));
		fi.mode = 0644 | S_IFREG;
		fi.name = "APKINDEX";
		counter = apk_ostream_counter(&fi.size);
		apk_db_index_write(db, counter);
		counter->close(counter);
		apk_tar_write_entry(os, &fi, NULL);
		total = apk_db_index_write(db, os);
		apk_tar_write_padding(os, &fi);

		apk_tar_write_entry(os, NULL, NULL);
	} else {
		total = apk_db_index_write(db, os);
	}
	os->close(os);

	apk_hash_foreach(&db->available.names, warn_if_no_providers, &counts);

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
	{ 'd', "description", "Embed TEXT as description and version "
	  "information of the repository index",
	  required_argument, "TEXT" },
	{ INDEX_OLD_FORMAT, "old-format",
	  "Specify to create old style index files" }
};

static struct apk_applet apk_index = {
	.name = "index",
	.help = "Create repository index file from FILEs.",
	.arguments = "FILE...",
	.open_flags = APK_OPENF_READ | APK_OPENF_NO_STATE | APK_OPENF_NO_REPOS,
	.context_size = sizeof(struct index_ctx),
	.num_options = ARRAY_SIZE(index_options),
	.options = index_options,
	.parse = index_parse,
	.main = index_main,
};

APK_DEFINE_APPLET(apk_index);

