/* index.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "apk_applet.h"
#include "apk_database.h"
#include "apk_print.h"

struct counts {
	int unsatisfied;
};

struct index_ctx {
	const char *index;
	const char *output;
	const char *description;
	apk_blob_t *rewrite_arch;
	time_t index_mtime;
	int method;
};

static int option_parse_applet(void *ctx, struct apk_db_options *dbopts, int optch, const char *optarg)
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
	case 0x10000:
		ictx->rewrite_arch = apk_blob_atomize(APK_BLOB_STR(optarg));
		break;
	default:
		return -ENOTSUP;
	}
	return 0;
}

static const struct apk_option options_applet[] = {
	{ 'o', "output", "Write the generated index to FILE",
	  required_argument, "FILE" },
	{ 'x', "index", "Read INDEX to speed up new index creation by reusing "
	  "the information from an old index",
	  required_argument, "INDEX" },
	{ 'd', "description", "Embed TEXT as description and version "
	  "information of the repository index",
	  required_argument, "TEXT" },
	{ 0x10000, "rewrite-arch", "Use ARCH as architecture for all packages",
	  required_argument, "ARCH" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "Index",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

static int index_read_file(struct apk_database *db, struct index_ctx *ictx)
{
	struct apk_file_info fi;

	if (ictx->index == NULL)
		return 0;
	if (apk_fileinfo_get(AT_FDCWD, ictx->index, APK_CHECKSUM_NONE, &fi) < 0)
		return 0;

	ictx->index_mtime = fi.mtime;
	return apk_db_index_read_file(db, ictx->index, 0);
}

static int warn_if_no_providers(apk_hash_item item, void *ctx)
{
	struct counts *counts = (struct counts *) ctx;
	struct apk_name *name = (struct apk_name *) item;

	if (!name->is_dependency) return 0;
	if (name->providers->num) return 0;

	if (++counts->unsatisfied < 10) {
		apk_warning("No provider for dependency '%s'",
			    name->name);
	} else if (counts->unsatisfied == 10) {
		apk_warning("Too many unsatisfiable dependencies, "
			    "not reporting the rest.");
	}

	return 0;
}

static int index_main(void *ctx, struct apk_database *db, struct apk_string_array *args)
{
	struct counts counts = {0};
	struct apk_ostream *os;
	struct apk_file_info fi;
	int total, r, found, newpkgs = 0, errors = 0;
	struct index_ctx *ictx = (struct index_ctx *) ctx;
	struct apk_package *pkg;
	char **parg;

	if (isatty(STDOUT_FILENO) && ictx->output == NULL &&
	    !(apk_force & APK_FORCE_BINARY_STDOUT)) {
		apk_error("Will not write binary index to console. "
			  "Use --force-binary-stdout to override.");
		return -1;
	}

	if (ictx->method == 0)
		ictx->method = APK_SIGN_GENERATE;

	if ((r = index_read_file(db, ictx)) < 0) {
		apk_error("%s: %s", ictx->index, apk_error_str(r));
		return r;
	}

	foreach_array_item(parg, args) {
		if (apk_fileinfo_get(AT_FDCWD, *parg, APK_CHECKSUM_NONE, &fi) < 0) {
			apk_warning("File '%s' is unaccessible", *parg);
			continue;
		}

		found = FALSE;
		do {
			struct apk_provider *p;
			struct apk_name *name;
			char *fname, *fend;
			apk_blob_t bname, bver;

			/* Check if index is newer than package */
			if (ictx->index == NULL || ictx->index_mtime < fi.mtime)
				break;

			/* Check that it looks like a package name */
			fname = strrchr(*parg, '/');
			if (fname == NULL)
				fname = *parg;
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

			foreach_array_item(p, name->providers) {
				pkg = p->pkg;
				if (pkg->name != name)
					continue;
				if (apk_blob_compare(bver, *pkg->version) != 0)
					continue;
				if (pkg->size != fi.size)
					continue;
				pkg->filename = strdup(*parg);
				if (ictx->rewrite_arch != NULL)
					pkg->arch = ictx->rewrite_arch;
				found = TRUE;
				break;
			}
		} while (0);

		if (!found) {
			struct apk_sign_ctx sctx;
			apk_sign_ctx_init(&sctx, ictx->method, NULL, db->keys_fd);
			r = apk_pkg_read(db, *parg, &sctx, &pkg);
			if (r < 0) {
				apk_error("%s: %s", *parg, apk_error_str(r));
				errors++;
			} else {
				newpkgs++;
				if (ictx->rewrite_arch != NULL)
					pkg->arch = ictx->rewrite_arch;
			}
			apk_sign_ctx_free(&sctx);
		}
	}
	if (errors)
		return -1;

	if (ictx->output != NULL)
		os = apk_ostream_to_file(AT_FDCWD, ictx->output, NULL, 0644);
	else
		os = apk_ostream_to_fd(STDOUT_FILENO);
	if (IS_ERR_OR_NULL(os)) return -1;

	if (ictx->method == APK_SIGN_GENERATE) {
		struct apk_ostream *counter;

		memset(&fi, 0, sizeof(fi));
		fi.mode = 0644 | S_IFREG;
		fi.name = "APKINDEX";
		counter = apk_ostream_counter(&fi.size);
		r = apk_db_index_write(db, counter);
		apk_ostream_close(counter);

		if (r >= 0) {
			os = apk_ostream_gzip(os);
			if (ictx->description != NULL) {
				struct apk_file_info fi_desc;
				memset(&fi_desc, 0, sizeof(fi));
				fi_desc.mode = 0644 | S_IFREG;
				fi_desc.name = "DESCRIPTION";
				fi_desc.size = strlen(ictx->description);
				apk_tar_write_entry(os, &fi_desc, ictx->description);
			}

			apk_tar_write_entry(os, &fi, NULL);
			r = apk_db_index_write(db, os);
			apk_tar_write_padding(os, &fi);

			apk_tar_write_entry(os, NULL, NULL);
		}
	} else {
		r = apk_db_index_write(db, os);
	}
	apk_ostream_close(os);

	if (r < 0) {
		apk_error("Index generation failed: %s", apk_error_str(r));
		return r;
	}

	total = r;
	apk_hash_foreach(&db->available.names, warn_if_no_providers, &counts);

	if (counts.unsatisfied != 0)
		apk_warning("Total of %d unsatisfiable package "
			    "names. Your repository may be broken.",
			    counts.unsatisfied);
	apk_message("Index has %d packages (of which %d are new)",
		    total, newpkgs);

	return 0;
}

static struct apk_applet apk_index = {
	.name = "index",
	.help = "Create repository index file from FILEs",
	.arguments = "FILE...",
	.open_flags = APK_OPENF_READ | APK_OPENF_NO_STATE | APK_OPENF_NO_REPOS,
	.context_size = sizeof(struct index_ctx),
	.optgroups = { &optgroup_global, &optgroup_applet },
	.main = index_main,
};

APK_DEFINE_APPLET(apk_index);

