/* apk_archive.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_ARCHIVE
#define APK_ARCHIVE

#include <sys/types.h>
#include "apk_blob.h"
#include "apk_io.h"

struct apk_archive_entry {
	char *name;
	char *link_target;
	char *uname; 
	char *gname;
	off_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	dev_t device;
};

typedef int (*apk_archive_entry_parser)(void *ctx,
					const struct apk_archive_entry *ae,
					struct apk_istream *istream);

struct apk_istream *apk_gunzip_bstream(struct apk_bstream *);

int apk_parse_tar(struct apk_istream *, apk_archive_entry_parser parser, void *ctx);
int apk_parse_tar_gz(struct apk_bstream *, apk_archive_entry_parser parser, void *ctx);

int apk_archive_entry_extract(const struct apk_archive_entry *ae,
			      struct apk_istream *is,
			      const char *to);

#endif
