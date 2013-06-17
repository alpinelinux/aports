/* print.c - Alpine Package Keeper (APK)
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
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "apk_defines.h"
#include "apk_print.h"

int apk_progress_fd;
static int apk_screen_width = 0;

void apk_reset_screen_width(void)
{
	apk_screen_width = 0;
}

int apk_get_screen_width(void)
{
	struct winsize w;

	if (apk_screen_width == 0) {
		apk_screen_width = 50;
		if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0 &&
		    w.ws_col > 50)
			apk_screen_width = w.ws_col;
	}

	return apk_screen_width;
}

void apk_print_progress(int percent_flags)
{
	static int last_written = 0;
	const int bar_width = apk_get_screen_width() - 7;
	char buf[8];
	int i, percent;

	percent = percent_flags & APK_PRINT_PROGRESS_MASK;

	if (last_written == percent && !(percent_flags & APK_PRINT_PROGRESS_FORCE))
		return;

	last_written = percent;

	if (apk_flags & APK_PROGRESS) {
		fprintf(stderr, "\e7%3i%% [", percent);
		for (i = 0; i < bar_width * percent / 100; i++)
			fputc('#', stderr);
		for (; i < bar_width; i++)
			fputc(' ', stderr);
		fputc(']', stderr);
		fflush(stderr);
		fputs("\e8\e[0K", stderr);
	}

	if (apk_progress_fd != 0) {
		i = snprintf(buf, sizeof(buf), "%zu\n", percent);
		write(apk_progress_fd, buf, i);
	}
}

int apk_print_indented(struct apk_indent *i, apk_blob_t blob)
{
	if (i->x + blob.len + 1 >= apk_get_screen_width())
		i->x = printf("\n%*s" BLOB_FMT, i->indent, "", BLOB_PRINTF(blob)) - 1;
	else if (i->x <= i->indent)
		i->x += printf("%*s" BLOB_FMT, i->indent - i->x, "", BLOB_PRINTF(blob));
	else
		i->x += printf(" " BLOB_FMT, BLOB_PRINTF(blob));
	return 0;
}

void apk_print_indented_words(struct apk_indent *i, const char *text)
{
	apk_blob_for_each_segment(APK_BLOB_STR(text), " ",
		(apk_blob_cb) apk_print_indented, i);
}

void apk_print_indented_fmt(struct apk_indent *i, const char *fmt, ...)
{
	char tmp[256];
	size_t n;
	va_list va;

	va_start(va, fmt);
	n = vsnprintf(tmp, sizeof(tmp), fmt, va);
	apk_print_indented(i, APK_BLOB_PTR_LEN(tmp, n));
	va_end(va);
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
	case ENOPKG:
		return "package not available";
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

