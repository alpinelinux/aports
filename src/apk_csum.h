/* apk_csum.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2009 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_CSUM_H
#define APK_CSUM_H

#include <openssl/evp.h>

#define MAX_CSUM_SIZE	16 /* MD5 */

typedef unsigned char *csum_p;
typedef unsigned char csum_t[MAX_CSUM_SIZE];
typedef EVP_MD_CTX csum_ctx_t;

extern csum_t bad_checksum;

static inline void csum_init(csum_ctx_t *ctx)
{
	EVP_DigestInit(ctx, EVP_md5());
}

static inline void csum_process(csum_ctx_t *ctx, unsigned char *buf, size_t len)
{
	EVP_DigestUpdate(ctx, buf, len);
}

static inline void csum_finish(csum_ctx_t *ctx, csum_p csum)
{
	EVP_DigestFinal(ctx, csum, NULL);
}

static inline int csum_valid(csum_p csum)
{
	return memcmp(csum, bad_checksum, MAX_CSUM_SIZE);
}

static inline void csum_blob(apk_blob_t blob, csum_p csum)
{
	csum_ctx_t ctx;

	csum_init(&ctx);
	csum_process(&ctx, (csum_p) blob.ptr, blob.len);
	csum_finish(&ctx, csum);
}

#endif
