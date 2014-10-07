/* gunzip.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <zlib.h>

#include "apk_defines.h"
#include "apk_io.h"

struct apk_gzip_istream {
	struct apk_istream is;
	struct apk_bstream *bs;
	z_stream zs;
	int err;

	apk_multipart_cb cb;
	void *cbctx;
	void *cbprev;
	apk_blob_t cbarg;
};

static ssize_t gzi_read(void *stream, void *ptr, size_t size)
{
	struct apk_gzip_istream *gis =
		container_of(stream, struct apk_gzip_istream, is);
	int r;

	if (gis->err != 0) {
		if (gis->err > 0)
			return 0;
		return gis->err;
	}

	if (ptr == NULL)
		return apk_istream_skip(&gis->is, size);

	gis->zs.avail_out = size;
	gis->zs.next_out  = ptr;

	while (gis->zs.avail_out != 0 && gis->err == 0) {
		if (!APK_BLOB_IS_NULL(gis->cbarg)) {
			r = gis->cb(gis->cbctx,
				    gis->err ? APK_MPART_END : APK_MPART_BOUNDARY,
				    gis->cbarg);
			if (r > 0)
				r = -ECANCELED;
			if (r != 0) {
				gis->err = r;
				goto ret;
			}
			gis->cbarg = APK_BLOB_NULL;
		}
		if (gis->zs.avail_in == 0) {
			apk_blob_t blob;

			if (gis->cb != NULL && gis->cbprev != NULL &&
			    gis->cbprev != gis->zs.next_in) {
				gis->cb(gis->cbctx, APK_MPART_DATA,
					APK_BLOB_PTR_LEN(gis->cbprev,
					(void *)gis->zs.next_in - gis->cbprev));
			}
			blob = gis->bs->read(gis->bs, APK_BLOB_NULL);
			gis->cbprev = blob.ptr;
			gis->zs.avail_in = blob.len;
			gis->zs.next_in = (void *) gis->cbprev;
			if (blob.len < 0) {
				gis->err = blob.len;
				goto ret;
			} else if (gis->zs.avail_in == 0) {
				gis->err = 1;
				if (gis->cb != NULL) {
					r = gis->cb(gis->cbctx, APK_MPART_END,
						    APK_BLOB_NULL);
					if (r > 0)
						r = -ECANCELED;
					if (r != 0)
						gis->err = r;
				}
				goto ret;
			}
		}

		r = inflate(&gis->zs, Z_NO_FLUSH);
		switch (r) {
		case Z_STREAM_END:
			/* Digest the inflated bytes */
			if ((gis->bs->flags & APK_BSTREAM_EOF) &&
			    gis->zs.avail_in == 0)
				gis->err = 1;
			if (gis->cb != NULL) {
				gis->cbarg = APK_BLOB_PTR_LEN(gis->cbprev, (void *) gis->zs.next_in - gis->cbprev); 
				gis->cbprev = gis->zs.next_in;
			}
			/* If we hit end of the bitstream (not end
			 * of just this gzip), we need to do the
			 * callback here, as we won't be called again.
			 * For boundaries it should be postponed to not
			 * be called until next gzip read is started. */
			if (gis->err) {
				r = gis->cb(gis->cbctx,
					    gis->err ? APK_MPART_END : APK_MPART_BOUNDARY,
					    gis->cbarg);
				if (r > 0)
					r = -ECANCELED;
				goto ret;
			}
			inflateEnd(&gis->zs);
			if (inflateInit2(&gis->zs, 15+32) != Z_OK)
				return -ENOMEM;
			break;
		case Z_OK:
			break;
		default:
			gis->err = -EIO;
			break;
		}
	}

ret:
	if (size - gis->zs.avail_out == 0)
		return gis->err < 0 ? gis->err : 0;

	return size - gis->zs.avail_out;
}

static void gzi_close(void *stream)
{
	struct apk_gzip_istream *gis =
		container_of(stream, struct apk_gzip_istream, is);

	inflateEnd(&gis->zs);
	gis->bs->close(gis->bs, NULL);
	free(gis);
}

struct apk_istream *apk_bstream_gunzip_mpart(struct apk_bstream *bs,
					     apk_multipart_cb cb, void *ctx)
{
	struct apk_gzip_istream *gis;

	if (!bs) return NULL;

	gis = malloc(sizeof(struct apk_gzip_istream));
	if (!gis) goto err;

	*gis = (struct apk_gzip_istream) {
		.is.read = gzi_read,
		.is.close = gzi_close,
		.bs = bs,
		.cb = cb,
		.cbctx = ctx,
	};

	if (inflateInit2(&gis->zs, 15+32) != Z_OK) {
		free(gis);
		goto err;
	}

	return &gis->is;
err:
	bs->close(bs, NULL);
	return NULL;
}

struct apk_gzip_ostream {
	struct apk_ostream os;
	struct apk_ostream *output;
	z_stream zs;
};

static ssize_t gzo_write(void *stream, const void *ptr, size_t size)
{
	struct apk_gzip_ostream *gos = (struct apk_gzip_ostream *) stream;
	unsigned char buffer[1024];
	ssize_t have, r;

	gos->zs.avail_in = size;
	gos->zs.next_in = (void *) ptr;
	while (gos->zs.avail_in) {
		gos->zs.avail_out = sizeof(buffer);
		gos->zs.next_out = buffer;
		r = deflate(&gos->zs, Z_NO_FLUSH);
		if (r == Z_STREAM_ERROR)
			return -EIO;
		have = sizeof(buffer) - gos->zs.avail_out;
		if (have != 0) {
			r = gos->output->write(gos->output, buffer, have);
			if (r != have)
				return -EIO;
		}
	}

	return size;
}

static int gzo_close(void *stream)
{
	struct apk_gzip_ostream *gos = (struct apk_gzip_ostream *) stream;
	unsigned char buffer[1024];
	size_t have;
	int r, rc = 0;

	do {
		gos->zs.avail_out = sizeof(buffer);
		gos->zs.next_out = buffer;
		r = deflate(&gos->zs, Z_FINISH);
		have = sizeof(buffer) - gos->zs.avail_out;
		if (gos->output->write(gos->output, buffer, have) != have)
			rc = -EIO;
	} while (r == Z_OK);
	r = gos->output->close(gos->output);
	if (r != 0)
		rc = r;

	deflateEnd(&gos->zs);
	free(stream);

	return rc;
}

struct apk_ostream *apk_ostream_gzip(struct apk_ostream *output)
{
	struct apk_gzip_ostream *gos;

	if (output == NULL)
		return NULL;

	gos = malloc(sizeof(struct apk_gzip_ostream));
	if (gos == NULL)
		goto err;

	*gos = (struct apk_gzip_ostream) {
		.os.write = gzo_write,
		.os.close = gzo_close,
		.output = output,
	};

	if (deflateInit2(&gos->zs, 9, Z_DEFLATED, 15 | 16, 8,
			 Z_DEFAULT_STRATEGY) != Z_OK) {
		free(gos);
		goto err;
	}

	return &gos->os;
err:
	output->close(output);
	return NULL;
}

