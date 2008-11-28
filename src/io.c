/* io.c - Alpine Package Keeper (APK)
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
#include <malloc.h>
#include <sys/mman.h>

#include "apk_defines.h"
#include "apk_io.h"

struct apk_fd_istream {
	struct apk_istream is;
	int fd;
};

static size_t fd_read(void *stream, void *ptr, size_t size)
{
	struct apk_fd_istream *fis =
		container_of(stream, struct apk_fd_istream, is);
	size_t i = 0, r;

	if (ptr == NULL) {
		if (lseek(fis->fd, size, SEEK_CUR) < 0)
			return -1;
		return size;
	}

	while (i < size) {
		r = read(fis->fd, ptr + i, size - i);
		if (r < 0)
			return r;
		if (r == 0)
			return i;
		i += r;
	}

	return i;
}

static void fd_close(void *stream)
{
	struct apk_fd_istream *fis =
		container_of(stream, struct apk_fd_istream, is);

	close(fis->fd);
	free(fis);
}

struct apk_istream *apk_istream_from_fd(int fd)
{
	struct apk_fd_istream *fis;

	if (fd < 0)
		return NULL;

	fis = malloc(sizeof(struct apk_fd_istream));
	if (fis == NULL)
		return NULL;

	*fis = (struct apk_fd_istream) {
		.is.read = fd_read,
		.is.close = fd_close,
		.fd = fd,
	};

	return &fis->is;
}

struct apk_istream *apk_istream_from_file(const char *file)
{
	int fd;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		return NULL;

	fcntl(fd, F_SETFD, FD_CLOEXEC);

	return apk_istream_from_fd(fd);
}

size_t apk_istream_skip(struct apk_istream *is, size_t size)
{
	unsigned char buf[2048];
	size_t done = 0, r, togo;

	while (done < size) {
		togo = size - done;
		if (togo > sizeof(buf))
			togo = sizeof(buf);
		r = is->read(is, buf, togo);
		if (r < 0)
			return r;
		done += r;
		if (r != togo)
			break;
	}
	return done;
}

size_t apk_istream_splice(void *stream, int fd, size_t size)
{
	struct apk_istream *is = (struct apk_istream *) stream;
	unsigned char *buf;
	size_t bufsz, done = 0, r, togo;

	bufsz = size;
	if (bufsz > 256*1024)
		bufsz = 256*1024;

	buf = malloc(bufsz);
	if (buf == NULL)
		return -1;

	while (done < size) {
		togo = size - done;
		if (togo > bufsz)
			togo = bufsz;
		r = is->read(is, buf, togo);
		if (r < 0)
			goto err;
		if (write(fd, buf, r) != r) {
			r = -1;
			goto err;
		}
		done += r;
		if (r != togo)
			break;
	}
	r = done;
err:
	free(buf);
	return r;
}

struct apk_istream_bstream {
	struct apk_bstream bs;
	struct apk_istream *is;
	csum_ctx_t csum_ctx;
	unsigned char buffer[8*1024];
};

static size_t is_bs_read(void *stream, void **ptr)
{
	struct apk_istream_bstream *isbs =
		container_of(stream, struct apk_istream_bstream, bs);
	size_t size;

	size = isbs->is->read(isbs->is, isbs->buffer, sizeof(isbs->buffer));
	if (size <= 0)
		return size;

	csum_process(&isbs->csum_ctx, isbs->buffer, size);

	*ptr = isbs->buffer;
	return size;
}

static void is_bs_close(void *stream, csum_t csum)
{
	struct apk_istream_bstream *isbs =
		container_of(stream, struct apk_istream_bstream, bs);

	if (csum != NULL) {
		size_t size;

		do {
			size = isbs->is->read(isbs->is, isbs->buffer,
					      sizeof(isbs->buffer));
			csum_process(&isbs->csum_ctx, isbs->buffer, size);
		} while (size == sizeof(isbs->buffer));

		csum_finish(&isbs->csum_ctx, csum);
	}

	isbs->is->close(isbs->is);
	free(isbs);
}

struct apk_bstream *apk_bstream_from_istream(struct apk_istream *istream)
{
	struct apk_istream_bstream *isbs;

	isbs = malloc(sizeof(struct apk_istream_bstream));
	if (isbs == NULL)
		return NULL;

	isbs->bs = (struct apk_bstream) {
		.read = is_bs_read,
		.close = is_bs_close,
	};
	isbs->is = istream;
	csum_init(&isbs->csum_ctx);

	return &isbs->bs;
}

struct apk_mmap_bstream {
	struct apk_bstream bs;
	csum_ctx_t csum_ctx;
	int fd;
	size_t size;
	unsigned char *ptr;
	size_t pos;
};

static size_t mmap_read(void *stream, void **ptr)
{
	struct apk_mmap_bstream *mbs =
		container_of(stream, struct apk_mmap_bstream, bs);
	size_t size;

	size = mbs->size - mbs->pos;
	if (size > 1024*1024)
		size = 1024*1024;

	*ptr = (void *) &mbs->ptr[mbs->pos];
	csum_process(&mbs->csum_ctx, &mbs->ptr[mbs->pos], size);
	mbs->pos += size;

	return size;
}

static void mmap_close(void *stream, csum_t csum)
{
	struct apk_mmap_bstream *mbs =
		container_of(stream, struct apk_mmap_bstream, bs);

	if (csum != NULL) {
		if (mbs->pos != mbs->size)
			csum_process(&mbs->csum_ctx, &mbs->ptr[mbs->pos],
				     mbs->size - mbs->pos);
		csum_finish(&mbs->csum_ctx, csum);
	}

	munmap(mbs->ptr, mbs->size);
	free(mbs);
}

static struct apk_bstream *apk_mmap_bstream_from_fd(int fd)
{
	struct apk_mmap_bstream *mbs;
	struct stat st;
	void *ptr;

	if (fstat(fd, &st) < 0)
		return NULL;

	ptr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED)
		return NULL;

	mbs = malloc(sizeof(struct apk_mmap_bstream));
	if (mbs == NULL) {
		munmap(ptr, st.st_size);
		return NULL;
	}

	mbs->bs = (struct apk_bstream) {
		.read = mmap_read,
		.close = mmap_close,
	};
	mbs->fd = fd;
	mbs->size = st.st_size;
	mbs->ptr = ptr;
	mbs->pos = 0;
	csum_init(&mbs->csum_ctx);

	return &mbs->bs;
}

struct apk_bstream *apk_bstream_from_fd(int fd)
{
	struct apk_bstream *bs;

	if (fd < 0)
		return NULL;

	bs = apk_mmap_bstream_from_fd(fd);
	if (bs != NULL)
		return bs;

	return apk_bstream_from_istream(apk_istream_from_fd(fd));
}

struct apk_bstream *apk_bstream_from_file(const char *file)
{
	int fd;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		return NULL;

	fcntl(fd, F_SETFD, FD_CLOEXEC);
	return apk_bstream_from_fd(fd);
}

apk_blob_t apk_blob_from_istream(struct apk_istream *is, size_t size)
{
	void *ptr;
	size_t rsize;

	ptr = malloc(size);
	if (ptr == NULL)
		return APK_BLOB_NULL;

	rsize = is->read(is, ptr, size);
	if (rsize < 0) {
		free(ptr);
		return APK_BLOB_NULL;
	}
	if (rsize != size)
		ptr = realloc(ptr, rsize);

	return APK_BLOB_PTR_LEN(ptr, rsize);
}

apk_blob_t apk_blob_from_file(const char *file)
{
	int fd;
	struct stat st;
	char *buf;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		return APK_BLOB_NULL;

	if (fstat(fd, &st) < 0)
		goto err_fd;

	buf = malloc(st.st_size);
	if (buf == NULL)
		goto err_fd;

	if (read(fd, buf, st.st_size) != st.st_size)
		goto err_read;

	close(fd);
	return APK_BLOB_PTR_LEN(buf, st.st_size);
err_read:
	free(buf);
err_fd:
	close(fd);
	return APK_BLOB_NULL;
}

int apk_file_get_info(const char *filename, struct apk_file_info *fi)
{
	struct stat st;
	struct apk_bstream *bs;

	if (stat(filename, &st) != 0)
		return -1;

	*fi = (struct apk_file_info) {
		.size = st.st_size,
		.uid = st.st_uid,
		.gid = st.st_gid,
		.mode = st.st_mode,
		.mtime = st.st_mtime,
		.device = st.st_dev,
	};

	bs = apk_bstream_from_file(filename);
	if (bs != NULL)
		bs->close(bs, fi->csum);

	return 0;
}

struct apk_istream *apk_istream_from_file_gz(const char *file)
{
	return apk_gunzip_bstream(apk_bstream_from_file(file));
}

