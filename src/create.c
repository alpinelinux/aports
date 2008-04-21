/* create.c - Alpine Package Keeper (APK)
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
#include "apk_applet.h"
#include "apk_database.h"

static int create_main(int argc, char **argv)
{
	return apk_db_create(apk_root);
}

static struct apk_applet apk_create = {
	.name = "create",
	.usage = "",
	.main = create_main,
};

APK_DEFINE_APPLET(apk_create);

