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

unsigned long apk_blob_hash(apk_blob_t blob)
{
	unsigned long hash = 5381;
	int i;

        for (i = 0; i < blob.len; i++)
		hash = hash * 33 + blob.ptr[i];

	return hash;
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
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 0xa;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 0xa;
	return -1;
}

unsigned int apk_blob_parse_uint(apk_blob_t *blob, int base)
{
	unsigned int val;
	int ch;

	val = 0;
	while (blob->len && blob->ptr[0] != 0) {
		ch = dx(blob->ptr[0]);
		if (ch < 0 || ch >= base)
			break;
		val *= base;
		val += ch;

		blob->ptr++;
		blob->len--;
	}

	return val;
}

int apk_blob_parse_char(apk_blob_t *blob)
{
	int r;

	if (blob->len == 0 || blob->ptr == NULL)
		return -1;
	r = blob->ptr[0];
	blob->ptr++;
	blob->len--;

	return r;
}

int apk_hexdump_parse(apk_blob_t to, apk_blob_t from)
{
	int i;

	if (to.len * 2 != from.len)
		return -1;

	for (i = 0; i < from.len / 2; i++)
		to.ptr[i] = (dx(from.ptr[i*2]) << 4) + dx(from.ptr[i*2+1]);

	return 0;
}

int apk_hexdump_format(int tolen, char *to, apk_blob_t from)
{
	static const char *xd = "0123456789abcdef";
	int i;

	for (i = 0; i < from.len && i*2+2 < tolen; i++) {
		to[i*2+0] = xd[(from.ptr[i] >> 4) & 0xf];
		to[i*2+1] = xd[from.ptr[i] & 0xf];
	}
	to[i*2] = 0;

	return i*2;
}
