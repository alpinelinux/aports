/*
 * Create mount directories in fstab
 *
 * Copyright(c) 2008 Natanael Copa <natanael.copa@gmail.com>
 * May be distributed under the terms of GPL-2
 *
 * usage: mkmntdirs [fstab]
 *
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <err.h>
#include <mntent.h>
#include <stdio.h>
#include <string.h>


#ifdef DEBUG
#define mkdir_recursive(p)	puts((p))
#else
static void mkdir_recursive(char *path)
{
	char *s = path;
	while (1) {
		int c = '\0';
		while (*s) {
			if (*s == '/') {
				do {
					++s;
				} while (*s == '/');
				c = *s; /* Save the current char */
				*s = '\0'; /* and replace it with nul. */
				break;
			}
			++s;
		}
		mkdir(path, 0755);
		if (c == '\0')
			return;
		*s = c;
	}
}
#endif

int main(int argc, const char *argv[])
{
	const char *filename = "/etc/fstab";
	FILE *f;
	struct mntent *ent;
	if (argc == 2)
		filename = argv[1];

	f = setmntent(filename, "r");
	if (f == NULL)
		err(1, "%s", filename);

	while ((ent = getmntent(f)) != NULL) {
		if (strcmp(ent->mnt_dir, "none") != 0)
			mkdir_recursive(ent->mnt_dir);
	}

	endmntent(f);
	return 0;
}

