/* url.c - Alpine Package Keeper (APK)
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
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include <fetch.h>

#include "apk_io.h"

const char *apk_url_local_file(const char *url)
{
	if (strncmp(url, "file:", 5) == 0)
		return &url[5];

	if (strncmp(url, "http:", 5) != 0 &&
	    strncmp(url, "https:", 6) != 0 &&
	    strncmp(url, "ftp:", 4) != 0)
		return url;

	return NULL;
}

struct apk_fetch_istream {
	struct apk_istream is;
	fetchIO *fetchIO;
};

static ssize_t fetch_read(void *stream, void *ptr, size_t size)
{
	struct apk_fetch_istream *fis = container_of(stream, struct apk_fetch_istream, is);
	ssize_t i = 0, r;

	if (ptr == NULL) return apk_istream_skip(&fis->is, size);

	while (i < size) {
		r = fetchIO_read(fis->fetchIO, ptr + i, size - i);
		if (r < 0) return -EIO;
		if (r == 0) break;
		i += r;
	}

	return i;
}

static void fetch_close(void *stream)
{
	struct apk_fetch_istream *fis = container_of(stream, struct apk_fetch_istream, is);

	fetchIO_close(fis->fetchIO);
	free(fis);
}

static struct apk_istream *apk_istream_fetch(const char *url, time_t since)
{
	struct apk_fetch_istream *fis;
	struct url *u;
	fetchIO *io;

	u = fetchParseURL(url);
	if (!u) return NULL;
	u->last_modified = since;
	io = fetchGet(u, "i");
	fetchFreeURL(u);
	if (!io) {
		if (fetchLastErrCode == FETCH_UNCHANGED) return ERR_PTR(-EALREADY);
		return NULL;
	}

	fis = malloc(sizeof(*fis));
	if (!fis) goto err;

	*fis = (struct apk_fetch_istream) {
		.is.read = fetch_read,
		.is.close = fetch_close,
		.fetchIO = io,
	};
	fetchFreeURL(u);

	return &fis->is;
err:
	if (io) fetchIO_close(io);
	return NULL;
}

struct apk_istream *apk_istream_from_fd_url_if_modified(int atfd, const char *url, time_t since)
{
	if (apk_url_local_file(url) != NULL)
		return apk_istream_from_file(atfd, apk_url_local_file(url));
	return apk_istream_fetch(url, since);
}

struct apk_istream *apk_istream_from_url_gz(const char *file)
{
	return apk_bstream_gunzip(apk_bstream_from_url(file));
}

struct apk_bstream *apk_bstream_from_fd_url_if_modified(int atfd, const char *url, time_t since)
{
	if (apk_url_local_file(url) != NULL)
		return apk_bstream_from_file(atfd, apk_url_local_file(url));
	return apk_bstream_from_istream(apk_istream_fetch(url, since));
}
