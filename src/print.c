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
static const char *apk_progress_char = "#";

void apk_reset_screen_width(void)
{
	apk_screen_width = 0;
	apk_progress_force = 1;
}

int apk_get_screen_width(void)
{
	struct winsize w;
	const char *lang;
	const char *progress_char;

	if (apk_screen_width == 0) {
		apk_screen_width = 50;
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 &&
		    w.ws_col > 50)
			apk_screen_width = w.ws_col;
	}

	lang = getenv("LANG");
	if (lang != NULL && strstr(lang, "UTF-8") != NULL)
		apk_progress_char = "\u2588";

	if ((progress_char = getenv("APK_PROGRESS_CHAR")) != NULL)
		apk_progress_char = progress_char;

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

	bar_width = apk_get_screen_width() - 6;
	if (total > 0) {
		bar = muldiv(bar_width, done, total);
		percent = muldiv(100, done, total);
	}

	if (bar  == last_bar && percent == last_percent && !apk_progress_force)
		return;

	last_bar = bar;
	last_percent = percent;
	apk_progress_force = 0;

	fprintf(stdout, "\e7%3i%% ", percent);

	for (i = 0; i < bar; i++)
		fputs(apk_progress_char, stdout);
	for (; i < bar_width; i++)
		fputc(' ', stdout);

	fflush(stdout);
	fputs("\e8\e[0K", stdout);
}

int apk_print_indented(struct apk_indent *i, apk_blob_t blob)
{
	if (i->x <= i->indent)
		i->x += printf("%*s" BLOB_FMT, i->indent - i->x, "", BLOB_PRINTF(blob));
	else if (i->x + blob.len + 1 >= apk_get_screen_width())
		i->x = printf("\n%*s" BLOB_FMT, i->indent, "", BLOB_PRINTF(blob)) - 1;
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
		return "could not find a repo which provides this package (check repositories file and run 'apk update')";
	case ECONNABORTED:
		return "network connection aborted";
	case ECONNREFUSED:
		return "could not connect to server (check repositories file)";
	case ENETUNREACH:
		return "network error (check Internet connection and firewall)";
	case ENXIO:
		return "DNS lookup error";
	case EREMOTEIO:
		return "remote server returned error (try 'apk update')";
	case ETIMEDOUT:
		return "operation timed out";
	case EAGAIN:
		return "temporary error (try again later)";
	case EAPKBADURL:
		return "invalid URL (check your repositories file)";
	case EAPKSTALEINDEX:
		return "package mentioned in index not found (try 'apk update')";
	default:
		return strerror(error);
	}
}

static void log_internal(FILE *dest, const char *prefix, const char *format, va_list va)
{
	if (dest != stdout)
		fflush(stdout);
	if (prefix != NULL)
		fprintf(dest, "%s", prefix);
	vfprintf(dest, format, va);
	fprintf(dest, "\n");
	fflush(dest);
	apk_progress_force = 1;
}

void apk_log(const char *prefix, const char *format, ...)
{
	va_list va;
	va_start(va, format);
	log_internal(stdout, prefix, format, va);
	va_end(va);
}

void apk_log_err(const char *prefix, const char *format, ...)
{
	va_list va;
	va_start(va, format);
	log_internal(stderr, prefix, format, va);
	va_end(va);
}
