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

static void process_match(struct apk_database *db, const char *match, struct apk_name *name, void *ctx)
{
	struct apk_provider *p;

	if (name == NULL)
		return;

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
        .open_flags = APK_OPENF_READ,
        .main = manifest_main,
};

APK_DEFINE_APPLET(apk_manifest);
