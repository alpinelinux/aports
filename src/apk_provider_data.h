/* apk_provider_data.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2012 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_PROVIDER_DATA_H
#define APK_PROVIDER_DATA_H

#include "apk_defines.h"
#include "apk_blob.h"

struct apk_provider {
	struct apk_package *pkg;
	apk_blob_t *version;
};
APK_ARRAY(apk_provider_array, struct apk_provider);

#define PROVIDER_FMT		"%s%s"BLOB_FMT
#define PROVIDER_PRINTF(n,p)	(n)->name, (p)->version->len ? "-" : "", BLOB_PRINTF(*(p)->version)

#endif
