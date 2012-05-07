/* apk_version.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_VERSION_H
#define APK_VERSION_H

#include "apk_blob.h"

#define APK_VERSION_UNKNOWN		0
#define APK_VERSION_EQUAL		1
#define APK_VERSION_LESS		2
#define APK_VERSION_GREATER		4

#define APK_DEPMASK_ANY		(APK_VERSION_EQUAL|APK_VERSION_LESS|\
				 APK_VERSION_GREATER)
#define APK_DEPMASK_CHECKSUM	(APK_VERSION_LESS|APK_VERSION_GREATER)

const char *apk_version_op_string(int result_mask);
int apk_version_result_mask(const char *str);
int apk_version_validate(apk_blob_t ver);
int apk_version_compare_blob(apk_blob_t a, apk_blob_t b);
int apk_version_compare(const char *str1, const char *str2);

#endif
