/* apk_io.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_IO
#define APK_IO

#include <sys/types.h>
#include <openssl/evp.h>
#include <fcntl.h>

#include "apk_defines.h"
#include "apk_blob.h"
#include "apk_hash.h"

struct apk_id_cache {
	int root_fd;
	unsigned int genid;
	struct apk_hash uid_cache;
	struct apk_hash gid_cache;
};

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
	struct apk_checksum csum;
};

struct apk_istream {
	ssize_t (*read)(void *stream, void *ptr, size_t size);
	void (*close)(void *stream);
};

#define APK_BSTREAM_SINGLE_READ			0x0001
#define APK_BSTREAM_EOF				0x0002

struct apk_bstream {
	unsigned int flags;
	apk_blob_t (*read)(void *stream, apk_blob_t token);
	void (*close)(void *stream, size_t *size);
};

struct apk_ostream {
	ssize_t (*write)(void *stream, const void *buf, size_t size);
	int (*close)(void *stream);
};

#define APK_MPART_DATA		1 /* data processed so far */
#define APK_MPART_BOUNDARY	2 /* final part of data, before boundary */
#define APK_MPART_END		3 /* signals end of stream */

typedef int (*apk_multipart_cb)(void *ctx, int part, apk_blob_t data);

struct apk_istream *apk_bstream_gunzip_mpart(struct apk_bstream *,
					     apk_multipart_cb cb, void *ctx);
static inline struct apk_istream *apk_bstream_gunzip(struct apk_bstream *bs)
{
	return apk_bstream_gunzip_mpart(bs, NULL, NULL);
}

struct apk_ostream *apk_ostream_gzip(struct apk_ostream *);
struct apk_ostream *apk_ostream_counter(off_t *);

struct apk_istream *apk_istream_from_fd_pid(int fd, pid_t pid, int (*translate_status)(int));
struct apk_istream *apk_istream_from_file(int atfd, const char *file);
struct apk_istream *apk_istream_from_file_gz(int atfd, const char *file);
struct apk_istream *apk_istream_from_fd_url(int atfd, const char *url);
struct apk_istream *apk_istream_from_url_gz(const char *url);
size_t apk_istream_skip(struct apk_istream *istream, size_t size);

#define APK_SPLICE_ALL 0xffffffff
size_t apk_istream_splice(void *stream, int fd, size_t size,
			  apk_progress_cb cb, void *cb_ctx);

static inline struct apk_istream *apk_istream_from_fd(int fd)
{
	return apk_istream_from_fd_pid(fd, 0, NULL);
}
static inline struct apk_istream *apk_istream_from_url(const char *url)
{
	return apk_istream_from_fd_url(AT_FDCWD, url);
}

struct apk_bstream *apk_bstream_from_istream(struct apk_istream *istream);
struct apk_bstream *apk_bstream_from_fd_pid(int fd, pid_t pid, int (*translate_status)(int));
struct apk_bstream *apk_bstream_from_file(int atfd, const char *file);
struct apk_bstream *apk_bstream_from_fd_url(int atfd, const char *url);
struct apk_bstream *apk_bstream_tee(struct apk_bstream *from, int atfd, const char *to,
				    apk_progress_cb cb, void *cb_ctx);

static inline struct apk_bstream *apk_bstream_from_fd(int fd)
{
	return apk_bstream_from_fd_pid(fd, 0, NULL);
}

static inline struct apk_bstream *apk_bstream_from_url(const char *url)
{
	return apk_bstream_from_fd_url(AT_FDCWD, url);
}

struct apk_ostream *apk_ostream_to_fd(int fd);
struct apk_ostream *apk_ostream_to_file(int atfd, const char *file, const char *tmpfile, mode_t mode);
struct apk_ostream *apk_ostream_to_file_gz(int atfd, const char *file, const char *tmpfile, mode_t mode);
size_t apk_ostream_write_string(struct apk_ostream *ostream, const char *string);

apk_blob_t apk_blob_from_istream(struct apk_istream *istream, size_t size);
apk_blob_t apk_blob_from_file(int atfd, const char *file);

#define APK_BTF_ADD_EOL		0x00000001
int apk_blob_to_file(int atfd, const char *file, apk_blob_t b, unsigned int flags);

#define APK_FI_NOFOLLOW		0x80000000
int apk_file_get_info(int atfd, const char *filename, unsigned int flags,
		      struct apk_file_info *fi);

typedef int apk_dir_file_cb(void *ctx, int dirfd, const char *entry);
int apk_dir_foreach_file(int dirfd, apk_dir_file_cb cb, void *ctx);

int apk_move_file(int atfd, const char *from, const char *to);
const char *apk_url_local_file(const char *url);

void apk_id_cache_init(struct apk_id_cache *idc, int root_fd);
void apk_id_cache_free(struct apk_id_cache *idc);
void apk_id_cache_reset(struct apk_id_cache *idc);
uid_t apk_resolve_uid(struct apk_id_cache *idc, const char *username, uid_t default_uid);
uid_t apk_resolve_gid(struct apk_id_cache *idc, const char *groupname, uid_t default_gid);

#endif
