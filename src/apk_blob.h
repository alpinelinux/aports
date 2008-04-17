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

struct apk_blob {
	unsigned int len;
	char *ptr;
};
typedef struct apk_blob apk_blob_t;

#define APK_BLOB_NULL			((apk_blob_t){0, NULL})
#define APK_BLOB_STR(str)		((apk_blob_t){strlen(str), (str)})
#define APK_BLOB_BUF(buf)		((apk_blob_t){sizeof(buf), (char *)(buf)})
#define APK_BLOB_PTR_LEN(beg,len)	((apk_blob_t){(len), (beg)})
#define APK_BLOB_PTR_PTR(beg,end)	APK_BLOB_PTR_LEN((beg),(end)-(beg)+1)

char *apk_blob_cstr(apk_blob_t str);
int apk_blob_splitstr(apk_blob_t blob, char *split, apk_blob_t *l, apk_blob_t *r);
int apk_blob_rsplit(apk_blob_t blob, char split, apk_blob_t *l, apk_blob_t *r);
unsigned apk_blob_uint(apk_blob_t blob, int base);

int apk_hexdump_parse(apk_blob_t to, apk_blob_t from);
int apk_hexdump_format(int tolen, char *to, apk_blob_t from);

#endif
