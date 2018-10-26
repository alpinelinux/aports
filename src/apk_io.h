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
#include <fcntl.h>
#include <time.h>

#include "apk_defines.h"
#include "apk_blob.h"
#include "apk_hash.h"

struct apk_id_cache {
	int root_fd;
	unsigned int genid;
	struct apk_hash uid_cache;
	struct apk_hash gid_cache;
};

struct apk_xattr {
	const char *name;
	apk_blob_t value;
};
APK_ARRAY(apk_xattr_array, struct apk_xattr);

struct apk_file_meta {
	time_t mtime, atime;
};
void apk_file_meta_to_fd(int fd, struct apk_file_meta *meta);

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
	struct apk_checksum xattr_csum;
	struct apk_xattr_array *xattrs;
};

struct apk_istream_ops {
	void (*get_meta)(void *stream, struct apk_file_meta *meta);
	ssize_t (*read)(void *stream, void *ptr, size_t size);
	void (*close)(void *stream);
};

struct apk_istream {
	const struct apk_istream_ops *ops;
};

#define APK_BSTREAM_SINGLE_READ			0x0001
#define APK_BSTREAM_EOF				0x0002

struct apk_bstream_ops {
	void (*get_meta)(void *stream, struct apk_file_meta *meta);
	apk_blob_t (*read)(void *stream, apk_blob_t token);
	void (*close)(void *stream, size_t *size);
};

struct apk_bstream {
	unsigned int flags;
	const struct apk_bstream_ops *ops;
};

struct apk_ostream_ops {
	ssize_t (*write)(void *stream, const void *buf, size_t size);
	int (*close)(void *stream);
};

struct apk_ostream {
	const struct apk_ostream_ops *ops;
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
struct apk_istream *apk_istream_from_fd_url_if_modified(int atfd, const char *url, time_t since);
struct apk_istream *apk_istream_from_url_gz(const char *url);
ssize_t apk_istream_skip(struct apk_istream *istream, size_t size);

#define APK_SPLICE_ALL 0xffffffff
ssize_t apk_istream_splice(void *stream, int fd, size_t size,
			   apk_progress_cb cb, void *cb_ctx);

static inline struct apk_istream *apk_istream_from_fd(int fd)
{
	return apk_istream_from_fd_pid(fd, 0, NULL);
}
static inline struct apk_istream *apk_istream_from_url(const char *url)
{
	return apk_istream_from_fd_url_if_modified(AT_FDCWD, url, 0);
}
static inline struct apk_istream *apk_istream_from_fd_url(int atfd, const char *url)
{
	return apk_istream_from_fd_url_if_modified(atfd, url, 0);
}
static inline struct apk_istream *apk_istream_from_url_if_modified(const char *url, time_t since)
{
	return apk_istream_from_fd_url_if_modified(AT_FDCWD, url, since);
}
static inline void apk_istream_get_meta(struct apk_istream *is, struct apk_file_meta *meta)
{
	is->ops->get_meta(is, meta);
}
static inline ssize_t apk_istream_read(struct apk_istream *is, void *ptr, size_t size)
{
	return is->ops->read(is, ptr, size);
}
static inline void apk_istream_close(struct apk_istream *is)
{
	is->ops->close(is);
}

struct apk_bstream *apk_bstream_from_istream(struct apk_istream *istream);
struct apk_bstream *apk_bstream_from_fd_pid(int fd, pid_t pid, int (*translate_status)(int));
struct apk_bstream *apk_bstream_from_file(int atfd, const char *file);
struct apk_bstream *apk_bstream_from_fd_url_if_modified(int atfd, const char *url, time_t since);
struct apk_bstream *apk_bstream_tee(struct apk_bstream *from, int atfd, const char *to, int copy_meta,
				    apk_progress_cb cb, void *cb_ctx);

static inline struct apk_bstream *apk_bstream_from_fd(int fd)
{
	return apk_bstream_from_fd_pid(fd, 0, NULL);
}

static inline struct apk_bstream *apk_bstream_from_url(const char *url)
{
	return apk_bstream_from_fd_url_if_modified(AT_FDCWD, url, 0);
}
static inline struct apk_bstream *apk_bstream_from_fd_url(int fd, const char *url)
{
	return apk_bstream_from_fd_url_if_modified(fd, url, 0);
}
static inline struct apk_bstream *apk_bstream_from_url_if_modified(const char *url, time_t since)
{
	return apk_bstream_from_fd_url_if_modified(AT_FDCWD, url, since);
}
static inline void apk_bstream_get_meta(struct apk_bstream *bs, struct apk_file_meta *meta)
{
	bs->ops->get_meta(bs, meta);
}
static inline apk_blob_t apk_bstream_read(struct apk_bstream *bs, apk_blob_t token)
{
	return bs->ops->read(bs, token);
}
static inline void apk_bstream_close(struct apk_bstream *bs, size_t *size)
{
	bs->ops->close(bs, size);
}

struct apk_ostream *apk_ostream_to_fd(int fd);
struct apk_ostream *apk_ostream_to_file(int atfd, const char *file, const char *tmpfile, mode_t mode);
struct apk_ostream *apk_ostream_to_file_gz(int atfd, const char *file, const char *tmpfile, mode_t mode);
size_t apk_ostream_write_string(struct apk_ostream *ostream, const char *string);
static inline ssize_t apk_ostream_write(struct apk_ostream *os, const void *buf, size_t size)
{
	return os->ops->write(os, buf, size);
}
static inline int apk_ostream_close(struct apk_ostream *os)
{
	return os->ops->close(os);
}

apk_blob_t apk_blob_from_istream(struct apk_istream *istream, size_t size);
apk_blob_t apk_blob_from_file(int atfd, const char *file);

#define APK_BTF_ADD_EOL		0x00000001
int apk_blob_to_file(int atfd, const char *file, apk_blob_t b, unsigned int flags);

#define APK_FI_NOFOLLOW		0x80000000
#define APK_FI_XATTR_CSUM(x)	(((x) & 0xff) << 8)
#define APK_FI_CSUM(x)		(((x) & 0xff))
int apk_fileinfo_get(int atfd, const char *filename, unsigned int flags,
		     struct apk_file_info *fi);
void apk_fileinfo_hash_xattr(struct apk_file_info *fi);
void apk_fileinfo_free(struct apk_file_info *fi);

typedef int apk_dir_file_cb(void *ctx, int dirfd, const char *entry);
int apk_dir_foreach_file(int dirfd, apk_dir_file_cb cb, void *ctx);

const char *apk_url_local_file(const char *url);

void apk_id_cache_init(struct apk_id_cache *idc, int root_fd);
void apk_id_cache_free(struct apk_id_cache *idc);
void apk_id_cache_reset(struct apk_id_cache *idc);
uid_t apk_resolve_uid(struct apk_id_cache *idc, const char *username, uid_t default_uid);
uid_t apk_resolve_gid(struct apk_id_cache *idc, const char *groupname, uid_t default_gid);

#endif
