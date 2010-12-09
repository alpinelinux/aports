/* print.c - Alpine Package Keeper (APK)
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
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>

#include "apk_defines.h"
#include "apk_print.h"

int apk_print_indented(struct apk_indent *i, apk_blob_t blob)
{
	static const int wrap_length = 80;

	if (i->x + blob.len + 1 >= wrap_length) {
		i->x = i->indent;
		printf("\n%*s", i->indent - 1, "");
	}
	i->x += printf(" %.*s", (int) blob.len, blob.ptr);
	return 0;
}

void apk_print_indented_words(struct apk_indent *i, const char *text)
{
	apk_blob_for_each_segment(APK_BLOB_STR(text), " ",
		(apk_blob_cb) apk_print_indented, i);
}

const char *apk_error_str(int error)
{
	if (error < 0)
		error = -error;
	switch (error) {
	case ENOKEY:
		return "UNTRUSTED signature";
	case EKEYREJECTED:
		return "BAD signature";
	case EIO:
		return "IO ERROR";
	case EBADMSG:
		return "BAD archive";
	case ENOMSG:
		return "archive does not contain expected data";
	default:
		return strerror(error);
	}
}

void apk_log(const char *prefix, const char *format, ...)
{
	va_list va;

	if (prefix != NULL)
		fprintf(stderr, "%s", prefix);
	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
	fprintf(stderr, "\n");
}

