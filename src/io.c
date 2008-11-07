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

static size_t fd_splice(void *stream, int fd, size_t size)
{
	struct apk_fd_istream *fis =
		container_of(stream, struct apk_fd_istream, is);
	size_t i = 0, r;

	while (i != size) {
		r = splice(fis->fd, NULL, fd, NULL, size - i, SPLICE_F_MOVE);
		if (r == -1)
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

	fis = malloc(sizeof(struct apk_fd_istream));
	if (fis == NULL)
		return NULL;

	*fis = (struct apk_fd_istream) {
		.is.read = fd_read,
		.is.splice = fd_splice,
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
	unsigned char buf[2048];
	size_t done = 0, r, togo;

	while (done < size) {
		togo = size - done;
		if (togo > sizeof(buf))
			togo = sizeof(buf);
		r = is->read(is, buf, togo);
		if (r < 0)
			return r;
		if (write(fd, buf, r) != r)
			return -1;
		done += r;
		if (r != togo)
			break;
	}
	return done;
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

	if (csum != NULL)
		csum_finish(&isbs->csum_ctx, csum);

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

struct apk_bstream *apk_bstream_from_fd(int fd)
{
	/* FIXME: Write mmap based bstream for files */
	return apk_bstream_from_istream(apk_istream_from_fd(fd));
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

