/* archive.c - Alpine Package Keeper (APK)
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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <utime.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "apk_defines.h"
#include "apk_print.h"
#include "apk_archive.h"

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

struct apk_tar_digest_info {
	char id[4];
	uint16_t nid;
	uint16_t size;
	unsigned char digest[];
};

#define GET_OCTAL(s)	get_octal(s, sizeof(s))
#define PUT_OCTAL(s,v)	put_octal(s, sizeof(s), v)

static int get_octal(char *s, size_t l)
{
	apk_blob_t b = APK_BLOB_PTR_LEN(s, l);
	return apk_blob_pull_uint(&b, 8);
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

struct apk_tar_entry_istream {
	struct apk_istream is;
	struct apk_istream *tar_is;
	size_t bytes_left;
	EVP_MD_CTX mdctx;
	struct apk_checksum *csum;
};

static ssize_t tar_entry_read(void *stream, void *ptr, size_t size)
{
	struct apk_tar_entry_istream *teis =
		container_of(stream, struct apk_tar_entry_istream, is);
	ssize_t r;

	if (size > teis->bytes_left)
		size = teis->bytes_left;
        if (size == 0)
                return 0;

	r = teis->tar_is->read(teis->tar_is, ptr, size);
	if (r < 0)
		return r;

	teis->bytes_left -= r;
	if (teis->csum == NULL)
		return r;

	EVP_DigestUpdate(&teis->mdctx, ptr, r);
	if (teis->bytes_left == 0) {
		teis->csum->type = EVP_MD_CTX_size(&teis->mdctx);
		EVP_DigestFinal_ex(&teis->mdctx, teis->csum->data, NULL);
	}
	return r;
}

static void tar_entry_close(void *stream)
{
}

int apk_tar_parse(struct apk_istream *is, apk_archive_entry_parser parser,
		  void *ctx, int soft_checksums, struct apk_id_cache *idc)
{
	struct apk_file_info entry;
	struct apk_tar_entry_istream teis = {
		.is.read = tar_entry_read,
		.is.close = tar_entry_close,
		.tar_is = is,
	};
	struct tar_header buf;
	struct apk_tar_digest_info *odi, *di;
	unsigned long offset = 0;
	int end = 0, r;
	size_t toskip;

	odi = (struct apk_tar_digest_info *) &buf.linkname[3];
	di  = (struct apk_tar_digest_info *) &buf.devmajor[0];
	EVP_MD_CTX_init(&teis.mdctx);
	memset(&entry, 0, sizeof(entry));
	while ((r = is->read(is, &buf, 512)) == 512) {
		offset += 512;
		if (buf.name[0] == '\0') {
			if (end)
				break;
			end++;
			continue;
		}

		entry = (struct apk_file_info){
			.size  = GET_OCTAL(buf.size),
			.uid   = apk_resolve_uid(idc, buf.uname, GET_OCTAL(buf.uid)),
			.gid   = apk_resolve_gid(idc, buf.gname, GET_OCTAL(buf.gid)),
			.mode  = GET_OCTAL(buf.mode) & 07777,
			.mtime = GET_OCTAL(buf.mtime),
			.name  = entry.name,
			.uname = buf.uname,
			.gname = buf.gname,
			.device = makedev(GET_OCTAL(buf.devmajor),
					  GET_OCTAL(buf.devminor)),
		};
		teis.csum = NULL;

		if (memcmp(di->id, "APK2", 4) == 0 &&
		    di->size <= sizeof(entry.csum.data)) {
			entry.csum.type = di->size;
			memcpy(&entry.csum.data[0], buf.devminor, sizeof(buf.devminor));
			memcpy(&entry.csum.data[sizeof(buf.devminor)], buf.padding, sizeof(buf.padding));
		}

		switch (buf.typeflag) {
		case 'L':
			if (entry.name != NULL)
				free(entry.name);
			entry.name = malloc(entry.size+1);
			is->read(is, entry.name, entry.size);
			entry.name[entry.size] = 0;
			offset += entry.size;
			entry.size = 0;
			break;
		case '0':
		case '7': /* regular file */
			entry.mode |= S_IFREG;
			if (entry.csum.type == APK_CHECKSUM_NONE) {
				if (memcmp(odi->id, "APK2", 4) == 0 &&
				    odi->size <= sizeof(entry.csum.data)) {
					entry.csum.type = odi->size;
					memcpy(entry.csum.data, odi->digest,
					       odi->size);
				} else if (soft_checksums)
					teis.csum = &entry.csum;
			}
			break;
		case '1': /* hard link */
			entry.mode |= S_IFREG;
			entry.link_target = buf.linkname;
			break;
		case '2': /* symbolic link */
			entry.mode |= S_IFLNK;
			entry.link_target = buf.linkname;
			if (entry.csum.type == APK_CHECKSUM_NONE &&
			    soft_checksums) {
				EVP_Digest(buf.linkname, strlen(buf.linkname),
					   entry.csum.data, NULL,
					   apk_checksum_default(), NULL);
				entry.csum.type = APK_CHECKSUM_DEFAULT;
			}
			break;
		case '3': /* char device */
			entry.mode |= S_IFCHR;
			break;
		case '4': /* block device */
			entry.mode |= S_IFBLK;
			break;
		case '5': /* directory */
			entry.mode |= S_IFDIR;
			break;
		default:
			break;
		}

		teis.bytes_left = entry.size;
		if (entry.mode & S_IFMT) {
			if (entry.name == NULL)
				entry.name = strdup(buf.name);

			/* callback parser function */
			if (teis.csum != NULL)
				EVP_DigestInit_ex(&teis.mdctx,
						  apk_checksum_default(), NULL);

			r = parser(ctx, &entry, &teis.is);
			free(entry.name);
			if (r != 0)
				goto err;

			entry.name = NULL;
		}

		offset += entry.size - teis.bytes_left;
		toskip = teis.bytes_left;
		if ((offset + toskip) & 511)
			toskip += 512 - ((offset + toskip) & 511);
		offset += toskip;
		if (toskip != 0)
			is->read(is, NULL, toskip);
	}
	EVP_MD_CTX_cleanup(&teis.mdctx);

	/* Read remaining end-of-archive records, to ensure we read all of
	 * the file. The underlying istream is likely doing checksumming. */
	if (r == 512) {
		while ((r = is->read(is, &buf, 512)) == 512) {
			if (buf.name[0] != 0)
				return -EBADMSG;
		}
	}

	/* Check that there was no partial record */
	if (r > 0)
		r = -EBADMSG;

	return r;

err:
	EVP_MD_CTX_cleanup(&teis.mdctx);
	return r;
}

int apk_tar_write_entry(struct apk_ostream *os, const struct apk_file_info *ae,
			const char *data)
{
	struct tar_header buf;

	memset(&buf, 0, sizeof(buf));
	if (ae != NULL) {
		const unsigned char *src;
	        int chksum, i;

		if (S_ISREG(ae->mode))
			buf.typeflag = '0';
		else
			return -1;

		if (ae->name != NULL)
			strncpy(buf.name, ae->name, sizeof(buf.name));

		strncpy(buf.uname, ae->uname ?: "root", sizeof(buf.uname));
		strncpy(buf.gname, ae->gname ?: "root", sizeof(buf.gname));

		PUT_OCTAL(buf.size, ae->size);
		PUT_OCTAL(buf.uid, ae->uid);
		PUT_OCTAL(buf.gid, ae->gid);
		PUT_OCTAL(buf.mode, ae->mode & 07777);
		PUT_OCTAL(buf.mtime, ae->mtime ?: time(NULL));

		/* Checksum */
		strcpy(buf.magic, "ustar  ");
		memset(buf.chksum, ' ', sizeof(buf.chksum));
		src = (const unsigned char *) &buf;
		for (i = chksum = 0; i < sizeof(buf); i++)
			chksum += src[i];
	        put_octal(buf.chksum, sizeof(buf.chksum)-1, chksum);
	}

	if (os->write(os, &buf, sizeof(buf)) != sizeof(buf))
		return -1;

	if (ae == NULL) {
		/* End-of-archive is two empty headers */
		if (os->write(os, &buf, sizeof(buf)) != sizeof(buf))
			return -1;
	} else if (data != NULL) {
		if (os->write(os, data, ae->size) != ae->size)
			return -1;
		if (apk_tar_write_padding(os, ae) != 0)
			return -1;
	}

	return 0;
}

int apk_tar_write_padding(struct apk_ostream *os, const struct apk_file_info *ae)
{
	static char padding[512];
	int pad;

	pad = 512 - (ae->size & 511);
	if (pad != 512 &&
	    os->write(os, padding, pad) != pad)
		return -1;

	return 0;
}

int apk_archive_entry_extract(int atfd, const struct apk_file_info *ae,
			      const char *suffix, struct apk_istream *is,
			      apk_progress_cb cb, void *cb_ctx)
{
	char *fn = ae->name;
	int fd, r = -1, atflags = 0;

	if (suffix != NULL) {
		fn = alloca(PATH_MAX);
		snprintf(fn, PATH_MAX, "%s%s", ae->name, suffix);
	}

	if ((!S_ISDIR(ae->mode) && !S_ISREG(ae->mode)) ||
	    (ae->link_target != NULL)) {
		/* non-standard entries need to be deleted first */
		unlinkat(atfd, fn, 0);
	}

	switch (ae->mode & S_IFMT) {
	case S_IFDIR:
		r = mkdirat(atfd, fn, ae->mode & 07777);
		if (r < 0 && errno == EEXIST)
			r = 0;
		break;
	case S_IFREG:
		if (ae->link_target == NULL) {
			int flags = O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC;

			fd = openat(atfd, fn, flags, ae->mode & 07777);
			if (fd < 0) {
				r = -1;
				break;
			}
			if (apk_istream_splice(is, fd, ae->size, cb, cb_ctx)
			    == ae->size)
				r = 0;
			close(fd);
		} else {
			char *link_target = ae->link_target;
			if (suffix != NULL) {
				link_target = alloca(PATH_MAX);
				snprintf(link_target, PATH_MAX, "%s%s",
					 ae->link_target, suffix);
			}
			r = linkat(atfd, link_target, atfd, fn, 0);
		}
		break;
	case S_IFLNK:
		r = symlinkat(ae->link_target, atfd, fn);
		atflags |= AT_SYMLINK_NOFOLLOW;
		break;
	case S_IFSOCK:
	case S_IFBLK:
	case S_IFCHR:
	case S_IFIFO:
		r = mknodat(atfd, fn, ae->mode & 07777, ae->device);
		break;
	}
	if (r == 0) {
		r = fchownat(atfd, fn, ae->uid, ae->gid, atflags);
		if (r < 0) {
			apk_error("Failed to set ownership on %s: %s",
				  fn, strerror(errno));
			return -errno;
		}

		/* chown resets suid bit so we need set it again */
		if (ae->mode & 07000) {
			r = fchmodat(atfd, fn, ae->mode & 07777, atflags);
			if (r < 0) {
				apk_error("Failed to set file permissions "
					  "on %s: %s",
					  fn, strerror(errno));
				return -errno;
			}
		}

		if (!S_ISLNK(ae->mode)) {
			/* preserve modification time */
			struct timespec times[2];

			times[0].tv_sec  = times[1].tv_sec  = ae->mtime;
			times[0].tv_nsec = times[1].tv_nsec = 0;
			r = utimensat(atfd, fn, times, atflags);
			if (r < 0) {
				apk_error("Failed to preserve modification time on %s: %s",
					fn, strerror(errno));
				return -errno;
			}
		}
	} else {
		apk_error("Failed to extract %s: %s", ae->name, strerror(errno));
		return -errno;
	}
	return 0;
}
