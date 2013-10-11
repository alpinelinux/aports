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
static int apk_progress_force = 1;

void apk_reset_screen_width(void)
{
	apk_screen_width = 0;
	apk_progress_force = 1;
}

int apk_get_screen_width(void)
{
	struct winsize w;

	if (apk_screen_width == 0) {
		apk_screen_width = 50;
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 &&
		    w.ws_col > 50)
			apk_screen_width = w.ws_col;
	}

	return apk_screen_width;
}

void apk_print_progress(size_t done, size_t total)
{
	static size_t last_done = 0;
	static int last_bar = 0, last_percent = 0;
	int bar_width;
	int bar = 0;
	char buf[64]; /* enough for petabytes... */
	int i, percent = 0;

	if (last_done == done && !apk_progress_force)
		return;

	if (apk_progress_fd != 0) {
		i = snprintf(buf, sizeof(buf), "%zu/%zu\n", done, total);
		write(apk_progress_fd, buf, i);
	}
	last_done = done;

	if (!(apk_flags & APK_PROGRESS))
		return;

	bar_width = apk_get_screen_width() - 7;
	if (total > 0) {
		bar = muldiv(bar_width, done, total);
		percent = muldiv(100, done, total);
	}

	if (bar  == last_bar && percent == last_percent && !apk_progress_force)
		return;

	last_bar = bar;
	last_percent = percent;
	apk_progress_force = 0;

	fprintf(stdout, "\e7%3i%% [", percent);
	for (i = 0; i < bar; i++)
		fputc('#', stdout);
	for (; i < bar_width; i++)
		fputc(' ', stdout);
	fputc(']', stdout);
	fflush(stdout);
	fputs("\e8\e[0K", stdout);
}

int apk_print_indented(struct apk_indent *i, apk_blob_t blob)
{
	if (i->x + blob.len + 1 >= apk_get_screen_width())
		i->x = printf("\n%*s" BLOB_FMT, i->indent, "", BLOB_PRINTF(blob)) - 1;
	else if (i->x <= i->indent)
		i->x += printf("%*s" BLOB_FMT, i->indent - i->x, "", BLOB_PRINTF(blob));
	else
		i->x += printf(" " BLOB_FMT, BLOB_PRINTF(blob));
	apk_progress_force = 1;
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
		fprintf(stdout, "%s", prefix);
	va_start(va, format);
	vfprintf(stdout, format, va);
	va_end(va);
	fprintf(stdout, "\n");
	apk_progress_force = 1;
}

