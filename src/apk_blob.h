/* apk_blob.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_BLOB_H
#define APK_BLOB_H

#include <string.h>
#include "apk_defines.h"

struct apk_blob {
	unsigned int len;
	char *ptr;
};
typedef struct apk_blob apk_blob_t;
typedef int (*apk_blob_cb)(void *ctx, apk_blob_t blob);

#define APK_BLOB_IS_NULL(blob)		((blob).ptr == NULL)

#define APK_BLOB_NULL			((apk_blob_t){0, NULL})
#define APK_BLOB_BUF(buf)		((apk_blob_t){sizeof(buf), (char *)(buf)})
#define APK_BLOB_PTR_LEN(beg,len)	((apk_blob_t){(len), (beg)})
#define APK_BLOB_PTR_PTR(beg,end)	APK_BLOB_PTR_LEN((beg),(end)-(beg)+1)

static inline apk_blob_t APK_BLOB_STR(const char *str)
{
	if (str == NULL)
		return APK_BLOB_NULL;
	return ((apk_blob_t){strlen(str), (void *)(str)});
}

char *apk_blob_cstr(apk_blob_t str);
int apk_blob_spn(apk_blob_t blob, const char *accept, apk_blob_t *l, apk_blob_t *r);
int apk_blob_cspn(apk_blob_t blob, const char *reject, apk_blob_t *l, apk_blob_t *r);
int apk_blob_split(apk_blob_t blob, apk_blob_t split, apk_blob_t *l, apk_blob_t *r);
int apk_blob_rsplit(apk_blob_t blob, char split, apk_blob_t *l, apk_blob_t *r);
unsigned long apk_blob_hash_seed(apk_blob_t, unsigned long seed);
unsigned long apk_blob_hash(apk_blob_t str);
int apk_blob_compare(apk_blob_t a, apk_blob_t b);
int apk_blob_for_each_segment(apk_blob_t blob, const char *split,
			      apk_blob_cb cb, void *ctx);

void apk_blob_push_blob(apk_blob_t *to, apk_blob_t literal);
void apk_blob_push_uint(apk_blob_t *to, unsigned int value, int radix);
void apk_blob_push_hexdump(apk_blob_t *to, apk_blob_t binary);

void apk_blob_pull_char(apk_blob_t *b, int expected);
unsigned int apk_blob_pull_uint(apk_blob_t *b, int radix);
void apk_blob_pull_hexdump(apk_blob_t *b, apk_blob_t to);

#endif
