/* common.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2010 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <malloc.h>
#include <string.h>
#include "apk_defines.h"

static int *dummy_array = 0;

void *apk_array_resize(void *array, size_t new_size, size_t elem_size)
{
	int old_size, diff;
	void *tmp;

	if (new_size == 0) {
		if (array != &dummy_array)
			free(array);
		return &dummy_array;
	}

	old_size = array ? *((int*) array) : 0;
	diff = new_size - old_size;

	if (array == &dummy_array)
		array = NULL;

	tmp = realloc(array, sizeof(int) + new_size * elem_size);
	if (diff > 0)
		memset(tmp + sizeof(int) + old_size * elem_size, 0,
		       diff * elem_size);
	*((int*) tmp) = new_size;

	return tmp;
}
