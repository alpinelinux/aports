/* archive.c - Alpine Package Keeper (APK)
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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/wait.h>

#include "apk_defines.h"
#include "apk_archive.h"

#ifndef GUNZIP_BINARY
#define GUNZIP_BINARY "/bin/gunzip"
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

static int get_dev_null(void)
{
	static int fd_null = 0;

	if (fd_null == 0) {
		fd_null = open("/dev/null", O_WRONLY);
		if (fd_null < 0)
			err(EX_OSFILE, "/dev/null");
	}
	return fd_null;

}

pid_t apk_open_gz(int *fd)
{
	int pipe_fd[2];
	pid_t child_pid;

	if (pipe(pipe_fd) < 0)
		err(EX_OSERR, "pipe");

	child_pid = fork();
	if (child_pid < 0)
		err(EX_OSERR, "fork");

	if (child_pid == 0) {
		close(pipe_fd[0]);
		dup2(pipe_fd[1], STDOUT_FILENO);
		dup2(*fd, STDIN_FILENO);
		dup2(get_dev_null(), STDERR_FILENO);
		close(pipe_fd[1]);
		execl(GUNZIP_BINARY, "gunzip", "-c", NULL);
		err(EX_UNAVAILABLE, GUNZIP_BINARY);
	}

	close(pipe_fd[1]);
	*fd = pipe_fd[0];

	return child_pid;
}

#define GET_OCTAL(s) apk_blob_uint(APK_BLOB_PTR_LEN(s, sizeof(s)), 8)

static int do_splice(int from_fd, int to_fd, int len)
{
	int i = 0, r;

	while (i != len) {
		r = splice(from_fd, NULL, to_fd, NULL, len - i, SPLICE_F_MOVE);
		if (r == -1)
			return i;
		i += r;
	}

	return i;
}

int apk_parse_tar(int fd, apk_archive_entry_parser parser, void *ctx)
{
	struct apk_archive_entry entry;
	struct tar_header buf;
	unsigned long offset = 0;
	int end = 0, r;

	memset(&entry, 0, sizeof(entry));
	while (read(fd, &buf, 512) == 512) {
		offset += 512;
		if (buf.name[0] == '\0') {
			if (end)
				break;
			end++;
			continue;
		}

		entry = (struct apk_archive_entry){
			.size  = GET_OCTAL(buf.size),
			.uid   = GET_OCTAL(buf.uid),
			.gid   = GET_OCTAL(buf.gid),
			.mode  = GET_OCTAL(buf.mode) & 0777,
			.mtime = GET_OCTAL(buf.mtime),
			.name  = entry.name,
			.uname = buf.uname,
			.gname = buf.gname,
			.device = makedev(GET_OCTAL(buf.devmajor),
					  GET_OCTAL(buf.devminor)),
			.read_fd = fd,
		};

		switch (buf.typeflag) {
		case 'L':
			if (entry.name != NULL)
				free(entry.name);
			entry.name = malloc(entry.size+1);
			read(fd, entry.name, entry.size);
			offset += entry.size;
			entry.size = 0;
			break;
		case '0':
		case '7': /* regular file */
			entry.mode |= S_IFREG;
			break;
		case '1': /* hard link */
			entry.mode |= S_IFREG;
			entry.link_target = buf.linkname;
			break;
		case '2': /* symbolic link */
			entry.mode |= S_IFLNK;
			entry.link_target = buf.linkname;
			break;
		case '3': /* char device */
			entry.mode |= S_IFCHR;
			break;
		case '4': /* block devicek */
			entry.mode |= S_IFBLK;
			break;
		case '5': /* directory */
			entry.mode |= S_IFDIR;
			break;
		default:
			break;
		}

		if (entry.mode & S_IFMT) {
			if (entry.name == NULL)
				entry.name = strdup(buf.name);

			/* callback parser function */
			offset += entry.size;
			r = parser(&entry, ctx);
			if (r != 0)
				return r;
			offset -= entry.size;

			free(entry.name);
			entry.name = NULL;
		}

		if (entry.size)
			offset += do_splice(fd, get_dev_null(), entry.size);

		/* align to next 512 block */
		if (offset & 511)
			offset += do_splice(fd, get_dev_null(),
					    512 - (offset & 511));
	}

	return 0;
}

int apk_parse_tar_gz(int fd, apk_archive_entry_parser parser, void *ctx)
{
	pid_t pid;
	int r, status;

	pid = apk_open_gz(&fd);
	if (pid < 0)
		return pid;

	r = apk_parse_tar(fd, parser, ctx);
	close(fd);
	waitpid(pid, &status, 0);

	return r;
}

apk_blob_t apk_archive_entry_read(struct apk_archive_entry *ae)
{
	char *str;
	int pos = 0;
	ssize_t r;

	str = malloc(ae->size + 1);
	pos = 0;
	while (ae->size) {
		r = read(ae->read_fd, &str[pos], ae->size);
		if (r < 0) {
			free(str);
			return APK_BLOB_NULL;
		}
		pos += r;
		ae->size -= r;
	}
	str[pos] = 0;

	return APK_BLOB_PTR_LEN(str, pos+1);
}

int apk_archive_entry_extract(struct apk_archive_entry *ae, const char *fn)
{
	int r = -1;

	if (fn == NULL)
		fn = ae->name;

	/* BIG HONKING FIXME */
	unlink(fn);

	switch (ae->mode & S_IFMT) {
	case S_IFDIR:
		r = mkdir(fn, ae->mode & 0777);
		if (r < 0 && errno == EEXIST)
			r = 0;
		break;
	case S_IFREG:
		if (ae->link_target == NULL) {
			r = open(fn, O_WRONLY | O_CREAT, ae->mode & 0777);
			if (r < 0)
				break;
			ae->size -= do_splice(ae->read_fd, r, ae->size);
			close(r);
			r = ae->size ? -1 : 0;
		} else {
			r = link(ae->link_target, fn);
		}
		break;
	case S_IFLNK:
		r = symlink(ae->link_target, fn);
		break;
	case S_IFSOCK:
	case S_IFBLK:
	case S_IFCHR:
	case S_IFIFO:
		r = mknod(fn, ae->mode, ae->device);
		break;
	}
	if (r != 0)
		apk_error("Failed to extract %s\n", ae->name);
	return r;
}

struct checksum_and_tee {
	int in_fd, tee_fd;
	void *ptr;
};

static void *__apk_checksum_and_tee(void *arg)
{
	struct checksum_and_tee *args = (struct checksum_and_tee *) arg;
	char buf[2*1024];
	int r, w, wt;
	__off64_t offset;
	csum_ctx_t ctx;
	int dosplice = 1;

	offset = lseek(args->in_fd, 0, SEEK_CUR);
	csum_init(&ctx);
	do {
		r = read(args->in_fd, buf, sizeof(buf));
		if (r <= 0)
			break;

		wt = 0;
		do {
			if (dosplice) {
				w = splice(args->in_fd, &offset, args->tee_fd, NULL,
					   r - wt, SPLICE_F_MOVE);
				if (w < 0) {
					dosplice = 0;
					continue;
				}
			} else {
				w = write(args->tee_fd, &buf[wt], r - wt);
				if (w < 0)
					break;
			}
			wt += w;
		} while (wt != r);

		csum_process(&ctx, buf, r);
	} while (r == sizeof(buf));

	csum_finish(&ctx, args->ptr);
	close(args->tee_fd);
	close(args->in_fd);
	free(args);

	return NULL;
}

pthread_t apk_checksum_and_tee(int *fd, void *ptr)
{
	struct checksum_and_tee *args;
	int fds[2];
	pthread_t tid;

	if (pipe(fds) < 0)
		return -1;

	fcntl(fds[0], F_SETFD, FD_CLOEXEC);
	fcntl(fds[1], F_SETFD, FD_CLOEXEC);

	args = malloc(sizeof(*args));
	*args = (struct checksum_and_tee){ *fd, fds[1], ptr };
	if (pthread_create(&tid, NULL, __apk_checksum_and_tee, args) < 0)
		return -1;

	*fd = fds[0];
	return tid;
}

