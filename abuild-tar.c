/* abuild-tar.c - A TAR mangling utility for .APK packages
 *
 * Copyright (C) 2009-2014 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/engine.h>

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
	char padding[12];   /* 500-511 */
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

static size_t get_octal(char *s, size_t l)
{
	size_t val;
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

static void tarhdr_checksum(struct tar_header *hdr)
{
	const unsigned char *src;
	size_t chksum, i;

	/* Recalculate checksum */
	memset(hdr->chksum, ' ', sizeof(hdr->chksum));
	src = (const unsigned char *) hdr;
	for (i = chksum = 0; i < sizeof(*hdr); i++)
		chksum += src[i];
	put_octal(hdr->chksum, sizeof(hdr->chksum)-1, chksum);
}

static int usage(void)
{
	fprintf(stderr,
"abuild-tar " VERSION "\n"
"\n"
"usage: abuild-tar [--hash[=<algorithm>]] [--cut]\n"
"\n"
"options:\n"
"  --hash[=sha1|md5]	Read tar archive from stdin, precalculate hash for \n"
"			regular	entries and output tar archive on stdout\n"
"  --cut		Remove the end of file tar record\n"
"\n");
	return 1;
}

static ssize_t full_read(int fd, void *buf, size_t count)
{
	ssize_t total, n;

	total = 0;
	do {
		n = read(fd, buf, count);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			break;
		buf += n;
		total += n;
		count -= n;
	} while (1);

	if (total == 0 && n < 0)
		return -errno;

	return total;
}

static ssize_t full_write(int fd, const void *buf, size_t count)
{
	ssize_t total, n;

	total = 0;
	do {
		n = write(fd, buf, count);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			break;
		buf += n;
		total += n;
		count -= n;
	} while (1);

	if (total == 0 && n < 0)
		return -errno;

	return total;
}

#if defined(__linux__)
static ssize_t full_splice(int from_fd, int to_fd, size_t count)
{
	ssize_t total, n;

	total = 0;
	do {
		n = splice(from_fd, NULL, to_fd, NULL, count, 0);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			break;
		count -= n;
		total += n;
	} while (1);

	if (total == 0 && n < 0)
		return -errno;

	return total;
}
#else
#define full_splice(from_fd, to_fd, count) -1
#endif

#define BUF_INITIALIZER {0}
struct buf {
	char *ptr;
	size_t size;
	size_t alloc;
};

static void buf_free(struct buf *b)
{
	free(b->ptr);
}

static int buf_resize(struct buf *b, size_t newsize)
{
	void *ptr;

	if (b->alloc >= newsize) return 0;
	ptr = realloc(b->ptr, newsize);
	if (!ptr) return -ENOMEM;
	b->ptr = ptr;
	b->alloc = newsize;
	return 0;
}

static int buf_padto(struct buf *b, size_t alignment)
{
	size_t oldsize, newsize;
	oldsize = b->size;
	newsize = (oldsize + alignment - 1) & -alignment;
	if (buf_resize(b, newsize)) return -ENOMEM;
	b->size = newsize;
	memset(b->ptr + oldsize, 0, newsize - oldsize);
	return 0;
}

static int buf_read_fd(struct buf *b, int fd, size_t size)
{
	ssize_t r;

	r = buf_resize(b, size);
	if (r) return r;

	r = full_read(fd, b->ptr, size);
	if (r == size) {
		b->size = r;
		return 0;
	}
	b->size = 0;
	if (r < 0) return r;
	return -EIO;
}

static int buf_write_fd(struct buf *b, int fd)
{
	ssize_t r;
	r = full_write(fd, b->ptr, b->size);
	if (r == b->size) return 0;
	if (r < 0) return r;
	return -ENOSPC;
}

static int buf_add_ext_header_hexdump(struct buf *b, const char *hdr, const unsigned char *value, int valuelen)
{
	int i, len;

	/* "%u %s=%s\n" */
	len = 1 + 1 + strlen(hdr) + 1 + valuelen*2 + 1;
	for (i = len; i > 9; i /= 10) len++;

	if (buf_resize(b, b->size + len)) return -ENOMEM;
	b->size += snprintf(&b->ptr[b->size], len, "%u %s=", len, hdr);
	for (i = 0; i < valuelen; i++)
		b->size += snprintf(&b->ptr[b->size], 3, "%02x", (int)value[i]);
	b->ptr[b->size++] = '\n';

	return 0;
}

static void add_legacy_checksum(struct tar_header *hdr, const EVP_MD *md, const unsigned char *digest)
{
	struct {
		char id[4];
		uint16_t nid;
		uint16_t size;
	} mdinfo;

	memcpy(mdinfo.id, "APK2", 4);
	mdinfo.nid = EVP_MD_nid(md);
	mdinfo.size = EVP_MD_size(md);
	memcpy(&hdr->linkname[3], &mdinfo, sizeof(mdinfo));
	memcpy(&hdr->linkname[3+sizeof(mdinfo)], digest, mdinfo.size);
	tarhdr_checksum(hdr);
}

static int do_it(const EVP_MD *md, int cut)
{
	char checksumhdr[32];
	unsigned char digest[EVP_MAX_MD_SIZE];
	struct buf data = BUF_INITIALIZER, pax = BUF_INITIALIZER;
	struct tar_header hdr, paxhdr;
	size_t size, aligned_size;
	int dohash = 0, r, ret = 1;

	memset(&paxhdr, 0, sizeof(paxhdr));

	if (md) snprintf(checksumhdr, sizeof(checksumhdr), "APK-TOOLS.checksum.%s", EVP_MD_name(md));

	do {
		if (full_read(STDIN_FILENO, &hdr, sizeof(hdr)) != sizeof(hdr))
			goto err;

		if (cut && hdr.name[0] == 0)
			break;

		size = GET_OCTAL(hdr.size);
		aligned_size = (size + 511) & ~511;

		if (hdr.typeflag == 'x') {
			memcpy(&paxhdr, &hdr, sizeof(hdr));
			if (buf_read_fd(&pax, STDIN_FILENO, aligned_size)) goto err;
			pax.size = size;
			continue;
		}

		dohash = md && (hdr.typeflag == '0' || hdr.typeflag == '7' || hdr.typeflag == '2');
		if (dohash) {
			if (buf_read_fd(&data, STDIN_FILENO, aligned_size)) goto err;

			if (hdr.typeflag != '2') {
				EVP_Digest(data.ptr, size, digest, NULL, md, NULL);
				add_legacy_checksum(&hdr, md, digest);
			} else {
				EVP_Digest(hdr.linkname, strlen(hdr.linkname), digest, NULL, md, NULL);
			}

			buf_add_ext_header_hexdump(&pax, checksumhdr, digest, EVP_MD_size(md));
			PUT_OCTAL(paxhdr.size, pax.size);
			tarhdr_checksum(&paxhdr);
		}

		if (pax.size) {
			/* write pax header + content */
			if (full_write(STDOUT_FILENO, &paxhdr, sizeof(paxhdr)) != sizeof(paxhdr) ||
			    buf_padto(&pax, 512) ||
			    buf_write_fd(&pax, STDOUT_FILENO)) goto err;
		}

		if (full_write(STDOUT_FILENO, &hdr, sizeof(hdr)) != sizeof(hdr)) goto err;

		if (dohash) {
			if (buf_write_fd(&data, STDOUT_FILENO)) goto err;
		} else if (aligned_size != 0) {
			r = full_splice(STDIN_FILENO, STDOUT_FILENO, aligned_size);
			if (r == -1) {
				while (aligned_size > 0) {
					if (full_read(STDIN_FILENO, &hdr, sizeof(hdr)) != sizeof(hdr))
						goto err;
					if (full_write(STDOUT_FILENO, &hdr, sizeof(hdr)) != sizeof(hdr))
						goto err;
					aligned_size -= sizeof(hdr);
				}
			} else if (r != aligned_size) goto err;
		}

		memset(&paxhdr, 0, sizeof(paxhdr));
		pax.size = 0;
	} while (1);

	ret = 0;
err:
	buf_free(&data);
	buf_free(&pax);
	return ret;
}

int main(int argc, char **argv)
{
	static int cut = 0;
	static const struct option options[] = {
		{ "hash", optional_argument },
		{ "cut", no_argument, &cut, 1 },
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
