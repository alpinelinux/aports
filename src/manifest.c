/* manifest.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2017 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2017 Timo Teräs <timo.teras@iki.fi>
 * Copyright (C) 2017 William Pitcock <nenolod@dereferenced.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include <sys/stat.h>

#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_version.h"
#include "apk_print.h"

/* TODO: support package files as well as generating manifest from the installed DB. */
static char *csum_types[APK_CHECKSUM_SHA1 + 1] = {
	[APK_CHECKSUM_MD5] = "md5",
	[APK_CHECKSUM_SHA1] = "sha1",
};

struct manifest_file_ctx {
	const char *file;
	struct apk_sign_ctx *sctx;
};

static void process_package(struct apk_database *db, struct apk_package *pkg)
{
	struct apk_installed_package *ipkg = pkg->ipkg;
	struct apk_db_dir_instance *diri;
	struct apk_db_file *file;
	struct hlist_node *dc, *dn, *fc, *fn;
	char csum_buf[(APK_CHECKSUM_SHA1 * 2) + 1];

	if (ipkg == NULL)
		return;

	hlist_for_each_entry_safe(diri, dc, dn, &ipkg->owned_dirs,
				  pkg_dirs_list) {
		hlist_for_each_entry_safe(file, fc, fn, &diri->owned_files,
					  diri_files_list) {
			apk_blob_t csum_blob = APK_BLOB_BUF(csum_buf);

			memset(csum_buf, '\0', sizeof(csum_buf));
			apk_blob_push_hexdump(&csum_blob, APK_BLOB_CSUM(file->csum));

			if (apk_verbosity > 1)
				printf("%s: ", pkg->name->name);

                        printf("%s:%s  " DIR_FILE_FMT "\n", csum_types[file->csum.type], csum_buf, DIR_FILE_PRINTF(diri->dir, file));
                }
	}
}

static int read_file_entry(void *ctx, const struct apk_file_info *ae,
                           struct apk_istream *is)
{
	struct manifest_file_ctx *mctx = ctx;
	char csum_buf[(APK_CHECKSUM_SHA1 * 2) + 1];
	apk_blob_t csum_blob = APK_BLOB_BUF(csum_buf);

	if (ae->name[0] == '.') {
		if (!strncmp(ae->name, ".PKGINFO", 8) || !strncmp(ae->name, ".SIGN.", 6))
			return 0;
	}

	if ((ae->mode & S_IFMT) != S_IFREG)
		return 0;

	memset(csum_buf, '\0', sizeof(csum_buf));
	apk_blob_push_hexdump(&csum_blob, APK_BLOB_CSUM(ae->csum));

	if (apk_verbosity > 1)
		printf("%s: ", mctx->file);

	printf("%s:%s  %s\n", csum_types[ae->csum.type], csum_buf, ae->name);

	return 0;
}

static void process_file(struct apk_database *db, const char *match)
{
	struct apk_sign_ctx sctx;
	struct apk_istream *is;
	struct manifest_file_ctx ctx = {match, &sctx};

	apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY, NULL, db->keys_fd);
	is = apk_bstream_gunzip_mpart(apk_bstream_from_file(AT_FDCWD, match),
				      apk_sign_ctx_mpart_cb, &sctx);

	if (IS_ERR_OR_NULL(is)) {
		apk_error("%s: %s", match, strerror(errno));
		return;
	}

	(void) apk_tar_parse(is, read_file_entry, &ctx, FALSE, &db->id_cache);
	apk_istream_close(is);
}

static void process_match(struct apk_database *db, const char *match, struct apk_name *name, void *ctx)
{
	struct apk_provider *p;

	if (name == NULL)
	{
		process_file(db, match);
		return;
	}

	foreach_array_item(p, name->providers)
		process_package(db, p->pkg);
}

static int manifest_main(void *ctx, struct apk_database *db, struct apk_string_array *args)
{
	apk_name_foreach_matching(db, args, apk_foreach_genid(), process_match, NULL);
	return 0;
}

static struct apk_applet apk_manifest = {
	.name = "manifest",
	.help = "Show checksums of package contents",
	.arguments = "PACKAGE...",
	.open_flags = APK_OPENF_READ,
	.command_groups = APK_COMMAND_GROUP_REPO,
	.main = manifest_main,
};

APK_DEFINE_APPLET(apk_manifest);
