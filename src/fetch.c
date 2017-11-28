/* fetch.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
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
#include "apk_io.h"
#include "apk_print.h"
#include "apk_solver.h"

#define FETCH_RECURSIVE		1
#define FETCH_STDOUT		2
#define FETCH_LINK		4

struct fetch_ctx {
	unsigned int flags;
	int outdir_fd, errors;
	struct apk_database *db;
	size_t done, total;
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

static int option_parse_applet(void *ctx, struct apk_db_options *dbopts, int optch, const char *optarg)
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
		return -ENOTSUP;
	}
	return 0;
}

static const struct apk_option options_applet[] = {
	{ 'L', "link",		"Create hard links if possible" },
	{ 'R', "recursive",	"Fetch the PACKAGE and all its dependencies" },
	{ 0x104, "simulate",	"Show what would be done without actually doing it" },
	{ 's', "stdout",	"Dump the .apk to stdout (incompatible "
				"with -o, -R, --progress)" },
	{ 'o', "output",	"Directory to place the PACKAGEs to",
	  required_argument, "DIR" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "Fetch",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

static void progress_cb(void *pctx, size_t bytes_done)
{
	struct fetch_ctx *ctx = (struct fetch_ctx *) pctx;
	apk_print_progress(ctx->done + bytes_done, ctx->total);
}

static int fetch_package(apk_hash_item item, void *pctx)
{
	struct fetch_ctx *ctx = (struct fetch_ctx *) pctx;
	struct apk_database *db = ctx->db;
	struct apk_package *pkg = (struct apk_package *) item;
	struct apk_istream *is;
	struct apk_repository *repo;
	struct apk_file_info fi;
	char url[PATH_MAX], filename[256];
	int r, fd, urlfd;

	if (!pkg->marked)
		return 0;

	repo = apk_db_select_repo(db, pkg);
	if (repo == NULL) {
		r = -ENOPKG;
		goto err;
	}

	if (snprintf(filename, sizeof(filename), PKG_FILE_FMT, PKG_FILE_PRINTF(pkg)) >= sizeof(filename)) {
		r = -ENOBUFS;
		goto err;
	}

	if (!(ctx->flags & FETCH_STDOUT)) {
		if (apk_fileinfo_get(ctx->outdir_fd, filename, APK_CHECKSUM_NONE, &fi) == 0 &&
		    fi.size == pkg->size)
			return 0;
	}

	apk_message("Downloading " PKG_VER_FMT, PKG_VER_PRINTF(pkg));
	if (apk_flags & APK_SIMULATE)
		return 0;

	r = apk_repo_format_item(db, repo, pkg, &urlfd, url, sizeof(url));
	if (r < 0)
		goto err;

	if (ctx->flags & FETCH_STDOUT) {
		fd = STDOUT_FILENO;
	} else {
		if ((ctx->flags & FETCH_LINK) && urlfd >= 0) {
			if (linkat(urlfd, url,
				   ctx->outdir_fd, filename,
				   AT_SYMLINK_FOLLOW) == 0)
				return 0;
		}
		fd = openat(ctx->outdir_fd, filename,
			    O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC, 0644);
		if (fd < 0) {
			r = -errno;
			goto err;
		}
	}

	is = apk_istream_from_fd_url(urlfd, url);
	if (IS_ERR_OR_NULL(is)) {
		r = PTR_ERR(is) ?: -EIO;
		goto err;
	}

	r = apk_istream_splice(is, fd, pkg->size, progress_cb, ctx);
	if (fd != STDOUT_FILENO) {
		struct apk_file_meta meta;
		apk_istream_get_meta(is, &meta);
		apk_file_meta_to_fd(fd, &meta);
		close(fd);
	}
	apk_istream_close(is);

	if (r != pkg->size) {
		unlinkat(ctx->outdir_fd, filename, 0);
		if (r >= 0) r = -EIO;
		goto err;
	}

	ctx->done += pkg->size;
	return 0;

err:
	apk_error(PKG_VER_FMT ": %s", PKG_VER_PRINTF(pkg), apk_error_str(r));
	ctx->errors++;
	return 0;
}

static void mark_package(struct fetch_ctx *ctx, struct apk_package *pkg)
{
	if (pkg == NULL || pkg->marked)
		return;
	ctx->total += pkg->size;
	pkg->marked = 1;
}

static void mark_error(struct fetch_ctx *ctx, const char *match, struct apk_name *name)
{
	if (strchr(match, '*') != NULL)
		return;

	apk_message("%s: unable to select package (or its dependencies)", name ? name->name : match);
	ctx->errors++;
}

static void mark_name_flags(struct apk_database *db, const char *match, struct apk_name *name, void *ctx)
{
	if (!IS_ERR_OR_NULL(name))
		name->auto_select_virtual = 1;
}

static void mark_name_recursive(struct apk_database *db, const char *match, struct apk_name *name, void *ctx)
{
	struct apk_changeset changeset = {};
	struct apk_dependency dep = (struct apk_dependency) {
		.name = name,
		.version = &apk_null_blob,
		.result_mask = APK_DEPMASK_ANY,
	};
	struct apk_dependency_array *world;
	struct apk_change *change;
	int r;

	if (!name) {
		mark_error(ctx, match, name);
		return;
	}

	apk_dependency_array_init(&world);
	*apk_dependency_array_add(&world) = dep;
	r = apk_solver_solve(db, 0, world, &changeset);
	if (r == 0) {
		foreach_array_item(change, changeset.changes)
			mark_package(ctx, change->new_pkg);
	} else {
		mark_error(ctx, match, name);
		if (apk_verbosity > 1)
			apk_solver_print_errors(db, &changeset, world);
	}
	apk_dependency_array_free(&world);

	apk_change_array_free(&changeset.changes);
}

static void mark_name(struct apk_database *db, const char *match, struct apk_name *name, void *ctx)
{
	struct apk_package *pkg = NULL;
	struct apk_provider *p;

	if (!name) goto err;

	foreach_array_item(p, name->providers)
		if (pkg == NULL || apk_pkg_version_compare(p->pkg, pkg) == APK_VERSION_GREATER)
			pkg = p->pkg;

	if (!pkg) goto err;
	mark_package(ctx, pkg);
	return;

err:
	mark_error(ctx, match, name);
}

static int purge_package(void *pctx, int dirfd, const char *filename)
{
	char tmp[PATH_MAX];
	struct fetch_ctx *ctx = (struct fetch_ctx *) pctx;
	struct apk_database *db = ctx->db;
	struct apk_provider *p0;
	struct apk_name *name;
	apk_blob_t b = APK_BLOB_STR(filename), bname, bver;
	size_t l;

	if (apk_pkg_parse_name(b, &bname, &bver)) return 0;
	name = apk_db_get_name(db, bname);
	if (!name) return 0;

	foreach_array_item(p0, name->providers) {
		if (p0->pkg->name != name) continue;
		l = snprintf(tmp, sizeof tmp, PKG_FILE_FMT, PKG_FILE_PRINTF(p0->pkg));
		if (l > sizeof tmp) continue;
		if (apk_blob_compare(b, APK_BLOB_PTR_LEN(tmp, l)) != 0) continue;
		if (p0->pkg->marked) return 0;
		break;
	}

	apk_message("Purging %s", filename);
	if (apk_flags & APK_SIMULATE)
		return 0;

	unlinkat(dirfd, filename, 0);
	return 0;
}

static int fetch_main(void *pctx, struct apk_database *db, struct apk_string_array *args)
{
	struct fetch_ctx *ctx = (struct fetch_ctx *) pctx;
	void *mark;

	if (ctx->flags & FETCH_STDOUT) {
		apk_flags &= ~APK_PROGRESS;
		apk_verbosity = 0;
	}

	if (ctx->outdir_fd == 0)
		ctx->outdir_fd = AT_FDCWD;

	if ((args->num == 1) && (strcmp(args->item[0], "coffee") == 0)) {
		if (apk_flags & APK_FORCE)
			return cup();
		apk_message("Go and fetch your own coffee.");
		return 0;
	}

	ctx->db = db;

	if (ctx->flags & FETCH_RECURSIVE) {
		apk_name_foreach_matching(db, args, apk_foreach_genid(), mark_name_flags, ctx);
		mark = mark_name_recursive;
	} else {
		mark = mark_name;
	}
	apk_name_foreach_matching(db, args, apk_foreach_genid(), mark, ctx);
	if (!ctx->errors)
		apk_hash_foreach(&db->available.packages, fetch_package, ctx);

	/* Remove packages not matching download spec from the output directory */
	if (!ctx->errors && (apk_flags & APK_PURGE) &&
	    !(ctx->flags & FETCH_STDOUT) && ctx->outdir_fd > 0)
		apk_dir_foreach_file(ctx->outdir_fd, purge_package, ctx);

	return ctx->errors;
}

static struct apk_applet apk_fetch = {
	.name = "fetch",
	.help = "Download PACKAGEs from global repositories to a local directory",
	.arguments = "PACKAGE...",
	.open_flags =	APK_OPENF_READ | APK_OPENF_NO_STATE,
	.context_size = sizeof(struct fetch_ctx),
	.optgroups = { &optgroup_global, &optgroup_applet },
	.main = fetch_main,
};

APK_DEFINE_APPLET(apk_fetch);

