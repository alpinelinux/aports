/* blob.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <malloc.h>
#include <string.h>

#include "apk_blob.h"

char *apk_blob_cstr(apk_blob_t blob)
{
	char *cstr;

	if (blob.ptr == NULL)
		return strdup("");

	if (blob.ptr[blob.len-1] == 0)
		return strdup(blob.ptr);

	cstr = malloc(blob.len + 1);
	memcpy(cstr, blob.ptr, blob.len);
	cstr[blob.len] = 0;

	return cstr;
}

int apk_blob_spn(apk_blob_t blob, const char *accept, apk_blob_t *l, apk_blob_t *r)
{
	int i;

	for (i = 0; i < blob.len; i++) {
		if (strchr(accept, blob.ptr[i]) == NULL) {
			*l = APK_BLOB_PTR_LEN(blob.ptr, i);
			*r = APK_BLOB_PTR_LEN(blob.ptr+i, blob.len-i);
			return 1;
		}
	}
	return 0;
}

int apk_blob_cspn(apk_blob_t blob, const char *reject, apk_blob_t *l, apk_blob_t *r)
{
	int i;

	for (i = 0; i < blob.len; i++) {
		if (strchr(reject, blob.ptr[i]) != NULL) {
			*l = APK_BLOB_PTR_LEN(blob.ptr, i);
			*r = APK_BLOB_PTR_LEN(blob.ptr+i, blob.len-i);
			return 1;
		}
	}
	return 0;
}

int apk_blob_rsplit(apk_blob_t blob, char split, apk_blob_t *l, apk_blob_t *r)
{
	char *sep;

	sep = memrchr(blob.ptr, split, blob.len);
	if (sep == NULL)
		return 0;

	if (l != NULL)
		*l = APK_BLOB_PTR_PTR(blob.ptr, sep - 1);
	if (r != NULL)
		*r = APK_BLOB_PTR_PTR(sep + 1, blob.ptr + blob.len - 1);

	return 1;
}

int apk_blob_split(apk_blob_t blob, apk_blob_t split, apk_blob_t *l, apk_blob_t *r)
{
	char *pos = blob.ptr, *end = blob.ptr + blob.len - split.len + 1;

	if (end < pos)
		return 0;

	while (1) {
		pos = memchr(pos, split.ptr[0], end - pos);
		if (pos == NULL)
			return 0;

		if (split.len > 1 && memcmp(pos, split.ptr, split.len) != 0) {
			pos++;
			continue;
		}

		*l = APK_BLOB_PTR_PTR(blob.ptr, pos-1);
		*r = APK_BLOB_PTR_PTR(pos+split.len, blob.ptr+blob.len-1);
		return 1;
	}
}

unsigned long apk_blob_hash_seed(apk_blob_t blob, unsigned long seed)
{
	unsigned long hash = seed;
	int i;

        for (i = 0; i < blob.len; i++)
		hash = hash * 33 + blob.ptr[i];

	return hash;
}

unsigned long apk_blob_hash(apk_blob_t blob)
{
	return apk_blob_hash_seed(blob, 5381);
}

int apk_blob_compare(apk_blob_t a, apk_blob_t b)
{
	if (a.len == b.len)
		return memcmp(a.ptr, b.ptr, a.len);
	if (a.len < b.len)
		return -1;
	return 1;
}

int apk_blob_for_each_segment(apk_blob_t blob, const char *split,
			      int (*cb)(void *ctx, apk_blob_t blob), void *ctx)
{
	apk_blob_t l, r, s = APK_BLOB_STR(split);
	int rc;

	r = blob;
	while (apk_blob_split(r, s, &l, &r)) {
		rc = cb(ctx, l);
		if (rc != 0)
			return rc;
	}
	if (r.len > 0)
		return cb(ctx, r);
	return 0;
}

static inline int dx(int c)
{
	if (likely(c >= '0' && c <= '9'))
		return c - '0';
	if (likely(c >= 'a' && c <= 'f'))
		return c - 'a' + 0xa;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 0xa;
	return -1;
}

void apk_blob_push_blob(apk_blob_t *to, apk_blob_t literal)
{
	if (unlikely(APK_BLOB_IS_NULL(*to)))
		return;

	if (unlikely(to->len < literal.len)) {
		*to = APK_BLOB_NULL;
		return;
	}

	memcpy(to->ptr, literal.ptr, literal.len);
	to->ptr += literal.len;
	to->len -= literal.len;
}

static const char *xd = "0123456789abcdefghijklmnopqrstuvwxyz";

void apk_blob_push_uint(apk_blob_t *to, unsigned int value, int radix)
{
	char buf[64];
	char *ptr = &buf[sizeof(buf)-1];

	if (value == 0) {
		apk_blob_push_blob(to, APK_BLOB_STR("0"));
		return;
	}

	while (value != 0) {
		*(ptr--) = xd[value % radix];
		value /= radix;
	}

	apk_blob_push_blob(to, APK_BLOB_PTR_PTR(ptr+1, &buf[sizeof(buf)-1]));
}

void apk_blob_push_hexdump(apk_blob_t *to, apk_blob_t binary)
{
	char *d;
	int i;

	if (unlikely(APK_BLOB_IS_NULL(*to)))
		return;

	if (unlikely(to->len < binary.len * 2)) {
		*to = APK_BLOB_NULL;
		return;
	}

	for (i = 0, d = to->ptr; i < binary.len; i++) {
		*(d++) = xd[(binary.ptr[i] >> 4) & 0xf];
		*(d++) = xd[binary.ptr[i] & 0xf];
	}
	to->ptr = d;
	to->len -= binary.len * 2;
}

void apk_blob_pull_char(apk_blob_t *b, int expected)
{
	if (unlikely(APK_BLOB_IS_NULL(*b)))
		return;
	if (unlikely(b->len < 1 || b->ptr[0] != expected)) {
		*b = APK_BLOB_NULL;
		return;
	}
	b->ptr ++;
	b->len --;
}

unsigned int apk_blob_pull_uint(apk_blob_t *b, int radix)
{
	unsigned int val;
	int ch;

	val = 0;
	while (b->len && b->ptr[0] != 0) {
		ch = dx(b->ptr[0]);
		if (ch < 0 || ch >= radix)
			break;
		val *= radix;
		val += ch;

		b->ptr++;
		b->len--;
	}

	return val;
}

void apk_blob_pull_hexdump(apk_blob_t *b, apk_blob_t to)
{
	char *s, *d;
	int i, r1, r2;

	if (unlikely(APK_BLOB_IS_NULL(*b)))
		return;

	if (unlikely(to.len > b->len * 2))
		goto err;

	for (i = 0, s = b->ptr, d = to.ptr; i < to.len; i++) {
		r1 = dx(*(s++));
		if (unlikely(r1 < 0))
			goto err;
		r2 = dx(*(s++));
		if (unlikely(r2 < 0))
			goto err;
		*(d++) = (r1 << 4) + r2;
	}
	b->ptr = s;
	b->len -= to.len * 2;
	return;
err:
	*b = APK_BLOB_NULL;
}
