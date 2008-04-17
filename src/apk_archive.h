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
#include <pthread.h>
#include "apk_blob.h"

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
	int read_fd;
};

typedef int (*apk_archive_entry_parser)(struct apk_archive_entry *entry, void *ctx);

pid_t apk_open_gz(int *fd);
int apk_parse_tar(int fd, apk_archive_entry_parser parser, void *ctx);
int apk_parse_tar_gz(int fd, apk_archive_entry_parser parser, void *ctx);
apk_blob_t apk_archive_entry_read(struct apk_archive_entry *ae);
int apk_archive_entry_extract(struct apk_archive_entry *ae, const char *to);
pthread_t apk_checksum_and_tee(int *fd, void *ptr);

#endif
