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
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <zlib.h>

#include "apk_applet.h"
#include "apk_database.h"
#include "apk_state.h"
#include "apk_io.h"
#include "apk_print.h"

#define FETCH_RECURSIVE		1
#define FETCH_STDOUT		2
#define FETCH_LINK		4

struct fetch_ctx {
	unsigned int flags;
	int outdir_fd;
};

static int cup(void)
{
	/* compressed/uncompressed size is 259/1213 */
	static unsigned char z[] = {
		0x78,0x9c,0x9d,0x94,0x3d,0x8e,0xc4,0x20,0x0c,0x85,0xfb,0x9c,
		0xc2,0x72,0x43,0x46,0x8a,0x4d,0x3f,0x67,0x89,0x64,0x77,0x2b,
		0x6d,0xbb,0x6d,0x0e,0x3f,0xc6,0x84,0x4d,0x08,0x84,0x55,0xd6,
		0xa2,0xe0,0xef,0x7b,0x36,0xe1,0x11,0x80,0x6e,0xcc,0x53,0x7f,
		0x3e,0xc5,0xeb,0xcf,0x1d,0x20,0x22,0xcc,0x3c,0x53,0x8e,0x17,
		0xd9,0x80,0x6d,0xee,0x0e,0x61,0x42,0x3c,0x8b,0xcf,0xc7,0x12,
		0x22,0x71,0x8b,0x31,0x05,0xd5,0xb0,0x11,0x4b,0xa7,0x32,0x2f,
		0x80,0x69,0x6b,0xb0,0x98,0x40,0xe2,0xcd,0xba,0x6a,0xba,0xe4,
		0x65,0xed,0x61,0x23,0x44,0xb5,0x95,0x06,0x8b,0xde,0x6c,0x61,
		0x70,0xde,0x0e,0xb6,0xed,0xc4,0x43,0x0c,0x56,0x6f,0x8f,0x31,
		0xd0,0x35,0xb5,0xc7,0x58,0x06,0xff,0x81,0x49,0x84,0xb8,0x0e,
		0xb1,0xd8,0xc1,0x66,0x31,0x0e,0x46,0x5c,0x43,0xc9,0xef,0xe5,
		0xdc,0x63,0xb1,0xdc,0x67,0x6d,0x31,0xb3,0xc9,0x69,0x74,0x87,
		0xc7,0xa3,0x1b,0x6a,0xb3,0xbd,0x2f,0x3b,0xd5,0x0c,0x57,0x3b,
		0xce,0x7c,0x5e,0xe5,0x48,0xd0,0x48,0x01,0x92,0x49,0x8b,0xf7,
		0xfc,0x58,0x67,0xb3,0xf7,0x14,0x20,0x5c,0x4c,0x9e,0xcc,0xeb,
		0x78,0x7e,0x64,0xa6,0xa1,0xf5,0xb2,0x70,0x38,0x09,0x7c,0x7f,
		0xfd,0xc0,0x8a,0x4e,0xc8,0x55,0xe8,0x12,0xe2,0x9f,0x1a,0xb1,
		0xb9,0x82,0x52,0x02,0x7a,0xe5,0xf9,0xd9,0x88,0x47,0x79,0x3b,
		0x46,0x61,0x27,0xf9,0x51,0xb1,0x17,0xb0,0x2c,0x0e,0xd5,0x39,
		0x2d,0x96,0x25,0x27,0xd6,0xd1,0x3f,0xa5,0x08,0xe1,0x9e,0x4e,
		0xa7,0xe9,0x03,0xb1,0x0a,0xb6,0x75
	};
	unsigned char buf[1213];
	unsigned long len = sizeof(buf);

	uncompress(buf, &len, z, sizeof(z));
	return write(STDOUT_FILENO, buf, len) != len;
}

static int fetch_parse(void *ctx, struct apk_db_options *dbopts,
		       int optch, int optindex, const char *optarg)
{
	struct fetch_ctx *fctx = (struct fetch_ctx *) ctx;

	switch (optch) {
	case 'R':
		fctx->flags |= FETCH_RECURSIVE;
		break;
	case 's':
		fctx->flags |= FETCH_STDOUT;
		break;
	case 'L':
		fctx->flags |= FETCH_LINK;
		break;
	case 'o':
		fctx->outdir_fd = openat(AT_FDCWD, optarg, O_RDONLY | O_CLOEXEC);
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
	struct apk_repository *repo;
	char pkgfile[PATH_MAX], url[PATH_MAX];
	int r, fd;

	apk_pkg_format_plain(pkg, APK_BLOB_BUF(pkgfile));

	if (!(fctx->flags & FETCH_STDOUT)) {
		struct apk_file_info fi;

		if (apk_file_get_info(fctx->outdir_fd, pkgfile,
				      APK_CHECKSUM_NONE, &fi) == 0 &&
		    fi.size == pkg->size)
			return 0;
	}

	apk_message("Downloading %s-%s", pkg->name->name, pkg->version);
	repo = apk_db_select_repo(db, pkg);
	if (repo == NULL) {
		apk_error("%s-%s: package is not currently available",
			  pkg->name->name, pkg->version);
		return -1;
	}

	if (apk_flags & APK_SIMULATE)
		return 0;

	snprintf(url, sizeof(url), "%s%s%s", repo->url,
		 repo->url[strlen(repo->url)-1] == '/' ? "" : "/",
		 pkgfile);

	if (fctx->flags & FETCH_STDOUT) {
		fd = STDOUT_FILENO;
	} else {
		if ((fctx->flags & FETCH_LINK) && apk_url_local_file(url)) {
			if (linkat(AT_FDCWD, url,
				   fctx->outdir_fd, pkgfile,
				   AT_SYMLINK_FOLLOW) == 0)
				return 0;
		}
		fd = openat(fctx->outdir_fd, pkgfile,
			    O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC, 0644);
		if (fd < 0) {
			apk_error("%s: %s", pkgfile, strerror(errno));
			return -1;
		}
	}

	is = apk_istream_from_url(url);
	if (is == NULL) {
		apk_error("Unable to download '%s'", url);
		return -1;
	}

	r = apk_istream_splice(is, fd, pkg->size, NULL, NULL);
	is->close(is);
	if (fd != STDOUT_FILENO)
		close(fd);
	if (r != pkg->size) {
		apk_error("Unable to download '%s'", url);
		unlinkat(fctx->outdir_fd, pkgfile, 0);
		return -1;
	}

	return 0;
}

static int fetch_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct fetch_ctx *fctx = (struct fetch_ctx *) ctx;
	int r = 0, i, j;

	if (fctx->outdir_fd == 0)
		fctx->outdir_fd = AT_FDCWD;

	if ((argc > 0) && (strcmp(argv[0], "coffee") == 0)) {
		if (apk_flags & APK_FORCE)
			return cup();
		apk_message("Go and fetch your own coffee.");
		return 0;
	}

	for (i = 0; i < argc; i++) {
		struct apk_dependency dep = (struct apk_dependency) {
			.name = apk_db_get_name(db, APK_BLOB_STR(argv[i])),
			.result_mask = APK_DEPMASK_REQUIRE,
		};

		if (fctx->flags & FETCH_RECURSIVE) {
			struct apk_state *state;
			struct apk_change *change;

			state = apk_state_new(db);
			if (state == NULL)
				goto err;

			r = apk_state_lock_dependency(state, &dep);
			if (r != 0) {
				apk_state_unref(state);
				apk_error("Unable to install '%s'",
					  dep.name->name);
				goto err;
			}

			list_for_each_entry(change, &state->change_list_head, change_list) {
				r = fetch_package(fctx, db, change->newpkg);
				if (r != 0)
					goto err;
			}

			apk_state_unref(state);
		} else {
			struct apk_package *pkg = NULL;

			for (j = 0; j < dep.name->pkgs->num; j++)
				if (pkg == NULL ||
				    apk_pkg_version_compare(dep.name->pkgs->item[j],
							    pkg)
				    == APK_VERSION_GREATER)
					pkg = dep.name->pkgs->item[j];

			if (pkg == NULL) {
				apk_message("Unable to get '%s'", dep.name->name);
				r = -1;
				break;
			}

			r = fetch_package(fctx, db, pkg);
			if (r != 0)
				goto err;
		}
	}

err:
	return r;
}

static struct apk_option fetch_options[] = {
	{ 'L', "link",		"Create hard links if possible" },
	{ 'R', "recursive",	"Fetch the PACKAGE and all it's dependencies" },
	{ 's', "stdout",
	  "Dump the .apk to stdout (incompatible with -o and -R)" },
	{ 'o', "output",	"Directory to place the PACKAGEs to",
	  required_argument, "DIR" },
};

static struct apk_applet apk_fetch = {
	.name = "fetch",
	.help = "Download PACKAGEs from global repositories to a local "
		"directory from which a local mirror repository can be "
		"created.",
	.arguments = "PACKAGE...",
	.open_flags =	APK_OPENF_READ | APK_OPENF_NO_STATE |
			APK_OPENF_NO_INSTALLED_REPO,
	.context_size = sizeof(struct fetch_ctx),
	.num_options = ARRAY_SIZE(fetch_options),
	.options = fetch_options,
	.parse = fetch_parse,
	.main = fetch_main,
};

APK_DEFINE_APPLET(apk_fetch);

