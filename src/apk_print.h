/* apk_print.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_PRINT_H
#define APK_PRINT_H

#include "apk_blob.h"

#define apk_error(args...)	do { apk_log("ERROR: ", args); } while (0)
#define apk_warning(args...)	do { if (apk_verbosity > 0) { apk_log("WARNING: ", args); } } while (0)
#define apk_message(args...)	do { if (apk_verbosity > 0) { apk_log(NULL, args); } } while (0)

void apk_log(const char *prefix, const char *format, ...);
const char *apk_error_str(int error);

void apk_reset_screen_width(void);
int apk_get_screen_width(void);

struct apk_indent {
	int x;
	int indent;
};

int apk_print_indented(struct apk_indent *i, apk_blob_t blob);
void apk_print_indented_words(struct apk_indent *i, const char *text);

#endif
