/* apk_io.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_IO
#define APK_IO

#include "apk_defines.h"
#include "apk_blob.h"

struct apk_file_info {
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
	csum_t csum;
};

struct apk_istream {
	size_t (*read)(void *stream, void *ptr, size_t size);
	void (*close)(void *stream);
};

struct apk_bstream {
	size_t (*read)(void *stream, void **ptr);
	void (*close)(void *stream, csum_p csum);
};

struct apk_istream *apk_gunzip_bstream(struct apk_bstream *);

struct apk_istream *apk_istream_from_fd(int fd);
struct apk_istream *apk_istream_from_file(const char *file);
size_t apk_istream_skip(struct apk_istream *istream, size_t size);
size_t apk_istream_splice(void *stream, int fd, size_t size);

struct apk_bstream *apk_bstream_from_istream(struct apk_istream *istream);
struct apk_bstream *apk_bstream_from_fd(int fd);

struct apk_istream *apk_istream_from_file_gz(const char *file);

apk_blob_t apk_blob_from_istream(struct apk_istream *istream, size_t size);
apk_blob_t apk_blob_from_file(const char *file);

#endif
