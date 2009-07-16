/* abuild-tar.c - A TAR mangling utility for .APK packages
 *
 * Copyright (C) 2009 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#include <openssl/evp.h>

#ifndef VERSION
#define VERSION ""
#endif

struct tar_header {
	/* ustar header, Posix 1003.1 */
	char name[100];     /*   0-99 */
	char mode[8];       /* 100-107 */
	char uid[8];        /* 108-115 */
	char gid[8];        /* 116-123 */
	char size[12];      /* 124-135 */
	char mtime[12];     /* 136-147 */
	char chksum[8];     /* 148-155 */
	char typeflag;      /* 156-156 */
	char linkname[100]; /* 157-256 */
	char magic[8];      /* 257-264 */
	char uname[32];     /* 265-296 */
	char gname[32];     /* 297-328 */
	char devmajor[8];   /* 329-336 */
	char devminor[8];   /* 337-344 */
	char prefix[155];   /* 345-499 */
	char padding[12];   /* 500-512 */
};

#define GET_OCTAL(s)	get_octal(s, sizeof(s))
#define PUT_OCTAL(s,v)	put_octal(s, sizeof(s), v)

static inline int dx(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 0xa;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 0xa;
	return -1;
}

static int get_octal(char *s, size_t l)
{
	unsigned int val;
	int ch;

	val = 0;
	while (l && s[0] != 0) {
		ch = dx(s[0]);
		if (ch < 0 || ch >= 8)
			break;
		val *= 8;
		val += ch;

		s++;
		l--;
	}

	return val;
}

static void put_octal(char *s, size_t l, size_t value)
{
	char *ptr = &s[l - 1];

	*(ptr--) = '\0';
	while (value != 0 && ptr >= s) {
		*(ptr--) = '0' + (value % 8);
		value /= 8;
	}
	while (ptr >= s)
		*(ptr--) = '0';
}

static int usage(void)
{
	fprintf(stderr,
"abuild-tar " VERSION "\n"
"\n"
"usage: abuild-tar [--hash [<algorithm>]] [--no-end]\n"
"\n"
"options:\n"
"  --hash [sha1|md5]	Read tar archive from stdin, precalculate hash for \n"
"			regular	entries and output tar archive on stdout\n"
"  --no-end		Remove the end of file tar record\n"
"\n");
}

static int do_it(const EVP_MD *md, int cut)
{
	struct tar_header hdr;
	size_t size, aligned_size;
	void *ptr;
	int dohash = 0, r;
	struct {
		char id[4];
		uint16_t nid;
		uint16_t size;
	} mdinfo;

	if (md != NULL) {
		memcpy(mdinfo.id, "APK2", 4);
		mdinfo.nid = EVP_MD_nid(md);
		mdinfo.size = EVP_MD_size(md);
	}

	do {
		if (read(STDIN_FILENO, &hdr, sizeof(hdr)) != sizeof(hdr))
			return 0;

		if (cut && hdr.name[0] == 0)
			return 0;

		size = GET_OCTAL(hdr.size);
		aligned_size = (size + 511) & ~511;

		if (md != NULL)
			dohash = (hdr.typeflag == '0' || hdr.typeflag == '7');
		if (dohash) {
			const unsigned char *src;
			int chksum, i;

			ptr = malloc(aligned_size);
			if (read(STDIN_FILENO, ptr, aligned_size) != aligned_size)
				return 1;

			memcpy(&hdr.linkname[3], &mdinfo, sizeof(mdinfo));
			EVP_Digest(ptr, size, &hdr.linkname[3+sizeof(mdinfo)],
				   NULL, md, NULL);

			/* Recalculate checksum */
			memset(hdr.chksum, ' ', sizeof(hdr.chksum));
			src = (const unsigned char *) &hdr;
			for (i = chksum = 0; i < sizeof(hdr); i++)
				chksum += src[i];
			put_octal(hdr.chksum, sizeof(hdr.chksum)-1, chksum);
		}

		if (write(STDOUT_FILENO, &hdr, sizeof(hdr)) != sizeof(hdr))
			return 2;

		if (dohash) {
			if (write(STDOUT_FILENO, ptr, aligned_size) != aligned_size)
				return 2;
			free(ptr);
		} else if (aligned_size != 0) {
			r = splice(STDIN_FILENO, NULL, STDOUT_FILENO, NULL,
				   aligned_size, 0);
			if (r == -1) {
				while (aligned_size > 0) {
					if (read(STDIN_FILENO, &hdr, sizeof(hdr)) != sizeof(hdr))
						return 1;
					if (write(STDOUT_FILENO, &hdr, sizeof(hdr)) != sizeof(hdr))
						return 2;
					aligned_size -= sizeof(hdr);
				}
			} else if (r != aligned_size)
				return 2;
		}
	} while (1);
}

int main(int argc, char **argv)
{
	static int cut = 0;
	static const struct option options[] = {
		{ "hash", optional_argument },
		{ "no-end", no_argument, &cut, 1 },
		{ NULL }
	};
	const EVP_MD *md = NULL;
	char *digest = NULL;
	int ndx;

	OpenSSL_add_all_algorithms();
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();

	while (getopt_long(argc, argv, "", options, &ndx) != -1) {
		if (ndx == 0)
			digest = optarg ? optarg : "sha1";
	}

	fprintf(stderr, "digest: %s, cut: %d\n", digest, cut);

	if (digest == NULL && cut == 0)
		return usage();
	if (isatty(STDIN_FILENO))
		return usage();

	if (digest != NULL) {
		md = EVP_get_digestbyname(digest);
		if (md == NULL)
			return usage();
	}

	return do_it(md, cut);
}
