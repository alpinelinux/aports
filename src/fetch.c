/* fetch.c - Alpine Package Keeper (APK)
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
#include "apk_state.h"
#include "apk_io.h"

#define FETCH_RECURSIVE		1
#define FETCH_STDOUT		2

struct fetch_ctx {
	unsigned int flags;
	const char *outdir;
};

static int fetch_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct fetch_ctx *fctx = (struct fetch_ctx *) ctx;

	switch (optch) {
	case 'R':
		fctx->flags |= FETCH_RECURSIVE;
		break;
	case 's':
		fctx->flags |= FETCH_STDOUT;
		break;
	case 'o':
		fctx->outdir = optarg;
		break;
	default:
		return -1;
	}
	return 0;
}

static int fetch_package(struct fetch_ctx *fctx,
			 struct apk_database *db,
			 struct apk_package *pkg)
{
	struct apk_istream *is;
	char file[256];
	int i, r, fd;

	if (fctx->flags & FETCH_STDOUT) {
		fd = STDOUT_FILENO;
	} else {
		struct apk_file_info fi;

		snprintf(file, sizeof(file), "%s/%s-%s.apk",
			 fctx->outdir ? fctx->outdir : ".",
			 pkg->name->name, pkg->version);

		if (apk_file_get_info(file, &fi) == 0 &&
		    fi.size == pkg->size)
			return 0;

		fd = creat(file, 0644);
		if (fd < 0) {
			apk_error("Unable to create '%s'", file);
			return -1;
		}
	}

	apk_message("Downloading %s-%s", pkg->name->name, pkg->version);

	for (i = 0; i < APK_MAX_REPOS; i++)
		if (pkg->repos & BIT(i))
			break;

	if (i >= APK_MAX_REPOS) {
		apk_error("%s-%s: not present in any repository",
			  pkg->name->name, pkg->version);
		return -1;
	}

	snprintf(file, sizeof(file), "%s/%s-%s.apk",
		 db->repos[i].url, pkg->name->name, pkg->version);
	is = apk_istream_from_url(file);
	if (is == NULL) {
		apk_error("Unable to download '%s'", file);
		return -1;
	}

	r = apk_istream_splice(is, fd, pkg->size, NULL, NULL);
	if (fd != STDOUT_FILENO)
		close(fd);
	if (r != pkg->size) {
		is->close(is);
		apk_error("Unable to download '%s'", file);
		unlink(file);
		return -1;
	}

	return 0;
}

static int fetch_main(void *ctx, int argc, char **argv)
{
	struct fetch_ctx *fctx = (struct fetch_ctx *) ctx;
	struct apk_database db;
	int i, j, r;

	r = apk_db_open(&db, apk_root, APK_OPENF_EMPTY_STATE);
	if (r != 0)
		return r;

	for (i = 0; i < argc; i++) {
		struct apk_dependency dep = (struct apk_dependency) {
			.name = apk_db_get_name(&db, APK_BLOB_STR(argv[i])),
			.result_mask = APK_DEPMASK_REQUIRE,
		};

		if (fctx->flags & FETCH_RECURSIVE) {
			struct apk_state *state;
			struct apk_change *change;

			state = apk_state_new(&db);
			r = apk_state_lock_dependency(state, &dep);
			if (r != 0) {
				apk_state_unref(state);
				apk_error("Unable to install '%s'",
					  dep.name->name);
				goto err;
			}

			list_for_each_entry(change, &state->change_list_head, change_list) {
				r = fetch_package(fctx, &db, change->newpkg);
				if (r != 0)
					goto err;
			}

			apk_state_unref(state);
		} else if (dep.name->pkgs != NULL) {
			struct apk_package *pkg = NULL;

			for (j = 0; j < dep.name->pkgs->num; j++)
				if (pkg == NULL ||
				    apk_version_compare(APK_BLOB_STR(dep.name->pkgs->item[j]->version),
							APK_BLOB_STR(pkg->version))
				    == APK_VERSION_GREATER)
					pkg = dep.name->pkgs->item[j];

			r = fetch_package(fctx, &db, pkg);
			if (r != 0)
				goto err;
		} else {
			apk_message("Unable to get '%s'", dep.name->name);
			r = -1;
			break;
		}
	}

err:
	apk_db_close(&db);
	return r;
}

static struct option fetch_options[] = {
	{ "recursive",	no_argument,		NULL, 'R' },
	{ "stdout",	no_argument,		NULL, 's' },
	{ "output",	required_argument,	NULL, 'o' },
};

static struct apk_applet apk_fetch = {
	.name = "fetch",
	.usage = "[-R|--recursive|--stdout] [-o dir] apkname...",
	.context_size = sizeof(struct fetch_ctx),
	.num_options = ARRAY_SIZE(fetch_options),
	.options = fetch_options,
	.parse = fetch_parse,
	.main = fetch_main,
};

APK_DEFINE_APPLET(apk_fetch);

