/* io.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <endian.h>
#include <unistd.h>
#include <malloc.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/param.h>
#include <pwd.h>
#include <grp.h>

#include "apk_defines.h"
#include "apk_io.h"
#include "apk_hash.h"
#include "apk_openssl.h"

#if defined(__GLIBC__) || defined(__UCLIBC__)
#define HAVE_FGETPWENT_R
#define HAVE_FGETGRENT_R
#endif

static void apk_file_meta_from_fd(int fd, struct apk_file_meta *meta)
{
	struct stat st;

	if (fstat(fd, &st) == 0) {
		meta->mtime = st.st_mtime;
		meta->atime = st.st_atime;
	} else {
		memset(meta, 0, sizeof(*meta));
	}
}

void apk_file_meta_to_fd(int fd, struct apk_file_meta *meta)
{
	struct timespec times[2] = {
		{ .tv_sec = meta->atime, .tv_nsec = meta->atime ? 0 : UTIME_OMIT },
		{ .tv_sec = meta->mtime, .tv_nsec = meta->mtime ? 0 : UTIME_OMIT }
	};
	futimens(fd, times);
}

struct apk_fd_istream {
	struct apk_istream is;
	int fd;
	pid_t pid;
	int (*translate_status)(int status);
};

static void fdi_get_meta(void *stream, struct apk_file_meta *meta)
{
	struct apk_fd_istream *fis =
		container_of(stream, struct apk_fd_istream, is);
	apk_file_meta_from_fd(fis->fd, meta);
}

static ssize_t fdi_read(void *stream, void *ptr, size_t size)
{
	struct apk_fd_istream *fis =
		container_of(stream, struct apk_fd_istream, is);
	ssize_t i = 0, r;

	if (ptr == NULL) {
		if (lseek(fis->fd, size, SEEK_CUR) < 0)
			return -errno;
		return size;
	}

	while (i < size) {
		r = read(fis->fd, ptr + i, size - i);
		if (r < 0)
			return -errno;
		if (r == 0)
			break;
		i += r;
	}
	if (i == 0 && fis->pid != 0) {
		int status;
		if (waitpid(fis->pid, &status, 0) == fis->pid)
			i = fis->translate_status(status);
		fis->pid = 0;
	}

	return i;
}

static void fdi_close(void *stream)
{
	struct apk_fd_istream *fis =
		container_of(stream, struct apk_fd_istream, is);
	int status;

	close(fis->fd);
	if (fis->pid != 0)
		waitpid(fis->pid, &status, 0);
	free(fis);
}

static const struct apk_istream_ops fd_istream_ops = {
	.get_meta = fdi_get_meta,
	.read = fdi_read,
	.close = fdi_close,
};

struct apk_istream *apk_istream_from_fd_pid(int fd, pid_t pid, int (*translate_status)(int))
{
	struct apk_fd_istream *fis;

	if (fd < 0) return ERR_PTR(-EBADF);

	fis = malloc(sizeof(struct apk_fd_istream));
	if (fis == NULL) {
		close(fd);
		return ERR_PTR(-ENOMEM);
	}

	*fis = (struct apk_fd_istream) {
		.is.ops = &fd_istream_ops,
		.fd = fd,
		.pid = pid,
		.translate_status = translate_status,
	};

	return &fis->is;
}

struct apk_istream *apk_istream_from_file(int atfd, const char *file)
{
	int fd;

	fd = openat(atfd, file, O_RDONLY | O_CLOEXEC);
	if (fd < 0) return ERR_PTR(-errno);

	return apk_istream_from_fd(fd);
}

ssize_t apk_istream_skip(struct apk_istream *is, size_t size)
{
	unsigned char buf[2048];
	size_t done = 0, togo;
	ssize_t r;

	while (done < size) {
		togo = MIN(size - done, sizeof buf);
		r = apk_istream_read(is, buf, togo);
		if (r <= 0) return r ?: -EIO;
		done += r;
	}
	return done;
}

ssize_t apk_istream_splice(void *stream, int fd, size_t size,
			   apk_progress_cb cb, void *cb_ctx)
{
	static void *splice_buffer = NULL;
	struct apk_istream *is = (struct apk_istream *) stream;
	unsigned char *buf, *mmapbase = MAP_FAILED;
	size_t bufsz, done = 0, togo;
	ssize_t r;

	bufsz = size;
	if (size > 128 * 1024) {
		if (size != APK_SPLICE_ALL) {
			r = posix_fallocate(fd, 0, size);
			if (r == 0)
				mmapbase = mmap(NULL, size, PROT_READ | PROT_WRITE,
						MAP_SHARED, fd, 0);
			else if (r == EBADF || r == EFBIG || r == ENOSPC || r == EIO)
				return -r;
		}
		bufsz = MIN(bufsz, 2*1024*1024);
		buf = mmapbase;
	}
	if (mmapbase == MAP_FAILED) {
		if (!splice_buffer) splice_buffer = malloc(256*1024);
		buf = splice_buffer;
		if (!buf) return -ENOMEM;
		bufsz = MIN(bufsz, 256*1024);
	}

	while (done < size) {
		if (cb != NULL) cb(cb_ctx, done);

		togo = MIN(size - done, bufsz);
		r = apk_istream_read(is, buf, togo);
		if (r <= 0) {
			if (r) goto err;
			if (size != APK_SPLICE_ALL && done != size) {
				r = -EBADMSG;
				goto err;
			}
			break;
		}

		if (mmapbase == MAP_FAILED) {
			if (write(fd, buf, r) != r) {
				if (r < 0)
					r = -errno;
				goto err;
			}
		} else
			buf += r;

		done += r;
	}
	r = done;
err:
	if (mmapbase != MAP_FAILED)
		munmap(mmapbase, size);
	return r;
}

struct apk_istream_bstream {
	struct apk_bstream bs;
	struct apk_istream *is;
	apk_blob_t left;
	char buffer[8*1024];
	size_t size;
};

static void is_bs_get_meta(void *stream, struct apk_file_meta *meta)
{
	struct apk_istream_bstream *isbs =
		container_of(stream, struct apk_istream_bstream, bs);
	return apk_istream_get_meta(isbs->is, meta);
}

static apk_blob_t is_bs_read(void *stream, apk_blob_t token)
{
	struct apk_istream_bstream *isbs =
		container_of(stream, struct apk_istream_bstream, bs);
	ssize_t size;
	apk_blob_t ret;

	/* If we have cached stuff, first check if it full fills the request */
	if (isbs->left.len != 0) {
		if (!APK_BLOB_IS_NULL(token)) {
			/* If we have tokenized thingy left, return it */
			if (apk_blob_split(isbs->left, token, &ret, &isbs->left))
				goto ret;
		} else
			goto ret_all;
	}

	/* If we've exchausted earlier, it's end of stream or error */
	if (APK_BLOB_IS_NULL(isbs->left))
		return isbs->left;

	/* We need more data */
	if (isbs->left.len != 0)
		memmove(isbs->buffer, isbs->left.ptr, isbs->left.len);
	isbs->left.ptr = isbs->buffer;
	size = apk_istream_read(isbs->is, isbs->buffer + isbs->left.len,
				sizeof(isbs->buffer) - isbs->left.len);
	if (size > 0) {
		isbs->size += size;
		isbs->left.len += size;
	} else if (size == 0) {
		if (isbs->left.len == 0)
			isbs->left = APK_BLOB_NULL;
		goto ret_all;
	} else {
		/* cache and return error */
		isbs->left = ret = APK_BLOB_ERROR(size);
		goto ret;
	}

	if (!APK_BLOB_IS_NULL(token)) {
		/* If we have tokenized thingy left, return it */
		if (apk_blob_split(isbs->left, token, &ret, &isbs->left))
			goto ret;
		/* No token found; just return the full buffer */
	}

ret_all:
	/* Return all that is in cache */
	ret = isbs->left;
	isbs->left.len = 0;
ret:
	return ret;
}

static void is_bs_close(void *stream, size_t *size)
{
	struct apk_istream_bstream *isbs =
		container_of(stream, struct apk_istream_bstream, bs);

	if (size != NULL)
		*size = isbs->size;

	apk_istream_close(isbs->is);
	free(isbs);
}

static const struct apk_bstream_ops is_bstream_ops = {
	.get_meta = is_bs_get_meta,
	.read = is_bs_read,
	.close = is_bs_close,
};

struct apk_bstream *apk_bstream_from_istream(struct apk_istream *istream)
{
	struct apk_istream_bstream *isbs;

	if (IS_ERR_OR_NULL(istream)) return ERR_CAST(istream);

	isbs = malloc(sizeof(struct apk_istream_bstream));
	if (isbs == NULL) return ERR_PTR(-ENOMEM);

	isbs->bs = (struct apk_bstream) {
		.ops = &is_bstream_ops,
	};
	isbs->is = istream;
	isbs->left = APK_BLOB_PTR_LEN(isbs->buffer, 0),
	isbs->size = 0;

	return &isbs->bs;
}

struct apk_mmap_bstream {
	struct apk_bstream bs;
	int fd;
	size_t size;
	unsigned char *ptr;
	apk_blob_t left;
};

static void mmap_get_meta(void *stream, struct apk_file_meta *meta)
{
	struct apk_mmap_bstream *mbs =
		container_of(stream, struct apk_mmap_bstream, bs);
	return apk_file_meta_from_fd(mbs->fd, meta);
}

static apk_blob_t mmap_read(void *stream, apk_blob_t token)
{
	struct apk_mmap_bstream *mbs =
		container_of(stream, struct apk_mmap_bstream, bs);
	apk_blob_t ret;

	if (!APK_BLOB_IS_NULL(token) && !APK_BLOB_IS_NULL(mbs->left)) {
		if (apk_blob_split(mbs->left, token, &ret, &mbs->left))
			return ret;
	}

	ret = mbs->left;
	mbs->left = APK_BLOB_NULL;
	mbs->bs.flags |= APK_BSTREAM_EOF;

	return ret;
}

static void mmap_close(void *stream, size_t *size)
{
	struct apk_mmap_bstream *mbs =
		container_of(stream, struct apk_mmap_bstream, bs);

	if (size != NULL)
		*size = mbs->size;
	munmap(mbs->ptr, mbs->size);
	close(mbs->fd);
	free(mbs);
}

static const struct apk_bstream_ops mmap_bstream_ops = {
	.get_meta = mmap_get_meta,
	.read = mmap_read,
	.close = mmap_close,
};

static struct apk_bstream *apk_mmap_bstream_from_fd(int fd)
{
	struct apk_mmap_bstream *mbs;
	struct stat st;
	void *ptr;

	if (fstat(fd, &st) < 0) return ERR_PTR(-errno);

	ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) return ERR_PTR(-errno);

	mbs = malloc(sizeof(struct apk_mmap_bstream));
	if (mbs == NULL) {
		munmap(ptr, st.st_size);
		return ERR_PTR(-ENOMEM);
	}

	mbs->bs = (struct apk_bstream) {
		.flags = APK_BSTREAM_SINGLE_READ,
		.ops = &mmap_bstream_ops,
	};
	mbs->fd = fd;
	mbs->size = st.st_size;
	mbs->ptr = ptr;
	mbs->left = APK_BLOB_PTR_LEN(ptr, mbs->size);

	return &mbs->bs;
}

struct apk_bstream *apk_bstream_from_fd_pid(int fd, pid_t pid, int (*translate_status)(int))
{
	struct apk_bstream *bs;

	if (fd < 0) return ERR_PTR(-EBADF);

	if (pid == 0) {
		bs = apk_mmap_bstream_from_fd(fd);
		if (!IS_ERR_OR_NULL(bs))
			return bs;
	}

	return apk_bstream_from_istream(apk_istream_from_fd_pid(fd, pid, translate_status));
}

struct apk_bstream *apk_bstream_from_file(int atfd, const char *file)
{
	int fd;

	fd = openat(atfd, file, O_RDONLY | O_CLOEXEC);
	if (fd < 0) return ERR_PTR(-errno);

	return apk_bstream_from_fd(fd);
}


struct apk_tee_bstream {
	struct apk_bstream bs;
	struct apk_bstream *inner_bs;
	int fd, copy_meta;
	size_t size;
	apk_progress_cb cb;
	void *cb_ctx;
};

static void tee_get_meta(void *stream, struct apk_file_meta *meta)
{
	struct apk_tee_bstream *tbs =
		container_of(stream, struct apk_tee_bstream, bs);
	apk_bstream_get_meta(tbs->inner_bs, meta);
}

static apk_blob_t tee_read(void *stream, apk_blob_t token)
{
	struct apk_tee_bstream *tbs =
		container_of(stream, struct apk_tee_bstream, bs);
	apk_blob_t blob;

	blob = apk_bstream_read(tbs->inner_bs, token);
	if (!APK_BLOB_IS_NULL(blob)) {
		tbs->size += write(tbs->fd, blob.ptr, blob.len);
		if (tbs->cb) tbs->cb(tbs->cb_ctx, tbs->size);
	}

	return blob;
}

static void tee_close(void *stream, size_t *size)
{
	struct apk_file_meta meta;
	struct apk_tee_bstream *tbs =
		container_of(stream, struct apk_tee_bstream, bs);

	if (tbs->copy_meta) {
		apk_bstream_get_meta(tbs->inner_bs, &meta);
		apk_file_meta_to_fd(tbs->fd, &meta);
	}

	apk_bstream_close(tbs->inner_bs, NULL);
	if (size != NULL) *size = tbs->size;
	close(tbs->fd);
	free(tbs);
}

static const struct apk_bstream_ops tee_bstream_ops = {
	.get_meta = tee_get_meta,
	.read = tee_read,
	.close = tee_close,
};

struct apk_bstream *apk_bstream_tee(struct apk_bstream *from, int atfd, const char *to, int copy_meta, apk_progress_cb cb, void *cb_ctx)
{
	struct apk_tee_bstream *tbs;
	int fd, r;

	if (IS_ERR_OR_NULL(from)) return ERR_CAST(from);

	fd = openat(atfd, to, O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		r = errno;
		apk_bstream_close(from, NULL);
		return ERR_PTR(-r);
	}

	tbs = malloc(sizeof(struct apk_tee_bstream));
	if (!tbs) {
		r = errno;
		close(fd);
		apk_bstream_close(from, NULL);
		return ERR_PTR(-r);
	}

	tbs->bs = (struct apk_bstream) {
		.ops = &tee_bstream_ops,
	};
	tbs->inner_bs = from;
	tbs->fd = fd;
	tbs->copy_meta = copy_meta;
	tbs->size = 0;
	tbs->cb = cb;
	tbs->cb_ctx = cb_ctx;

	return &tbs->bs;
}

apk_blob_t apk_blob_from_istream(struct apk_istream *is, size_t size)
{
	void *ptr;
	ssize_t rsize;

	ptr = malloc(size);
	if (ptr == NULL)
		return APK_BLOB_NULL;

	rsize = apk_istream_read(is, ptr, size);
	if (rsize < 0) {
		free(ptr);
		return APK_BLOB_NULL;
	}
	if (rsize != size)
		ptr = realloc(ptr, rsize);

	return APK_BLOB_PTR_LEN(ptr, rsize);
}

apk_blob_t apk_blob_from_file(int atfd, const char *file)
{
	int fd;
	struct stat st;
	char *buf;

	fd = openat(atfd, file, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return APK_BLOB_NULL;

	if (fstat(fd, &st) < 0)
		goto err_fd;

	buf = malloc(st.st_size);
	if (buf == NULL)
		goto err_fd;

	if (read(fd, buf, st.st_size) != st.st_size)
		goto err_read;

	close(fd);
	return APK_BLOB_PTR_LEN(buf, st.st_size);
err_read:
	free(buf);
err_fd:
	close(fd);
	return APK_BLOB_NULL;
}

int apk_blob_to_file(int atfd, const char *file, apk_blob_t b, unsigned int flags)
{
	int fd, r, len;

	fd = openat(atfd, file, O_CREAT | O_WRONLY | O_CLOEXEC, 0644);
	if (fd < 0)
		return -errno;

	len = b.len;
	r = write(fd, b.ptr, len);
	if ((r == len) &&
	    (flags & APK_BTF_ADD_EOL) && (b.len == 0 || b.ptr[b.len-1] != '\n')) {
		len = 1;
		r = write(fd, "\n", len);
	}

	if (r < 0)
		r = -errno;
	else if (r != len)
		r = -ENOSPC;
	else
		r = 0;
	close(fd);

	if (r != 0)
		unlinkat(atfd, file, 0);

	return r;
}

static int cmp_xattr(const void *p1, const void *p2)
{
	const struct apk_xattr *d1 = p1, *d2 = p2;
	return strcmp(d1->name, d2->name);
}

static void hash_len_data(EVP_MD_CTX *ctx, uint32_t len, const void *ptr)
{
	uint32_t belen = htobe32(len);
	EVP_DigestUpdate(ctx, &belen, sizeof(belen));
	EVP_DigestUpdate(ctx, ptr, len);
}

void apk_fileinfo_hash_xattr_array(struct apk_xattr_array *xattrs, const EVP_MD *md, struct apk_checksum *csum)
{
	struct apk_xattr *xattr;
	EVP_MD_CTX *mdctx;

	if (!xattrs || xattrs->num == 0) goto err;
	mdctx = EVP_MD_CTX_new();
	if (!mdctx) goto err;

	qsort(xattrs->item, xattrs->num, sizeof(xattrs->item[0]), cmp_xattr);

	EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
	foreach_array_item(xattr, xattrs) {
		hash_len_data(mdctx, strlen(xattr->name), xattr->name);
		hash_len_data(mdctx, xattr->value.len, xattr->value.ptr);
	}
	csum->type = EVP_MD_CTX_size(mdctx);
	EVP_DigestFinal_ex(mdctx, csum->data, NULL);
	EVP_MD_CTX_free(mdctx);
	return;
err:
	csum->type = APK_CHECKSUM_NONE;
}

void apk_fileinfo_hash_xattr(struct apk_file_info *fi)
{
	apk_fileinfo_hash_xattr_array(fi->xattrs, apk_checksum_default(), &fi->xattr_csum);
}

int apk_fileinfo_get(int atfd, const char *filename, unsigned int flags,
		     struct apk_file_info *fi)
{
	struct stat64 st;
	struct apk_bstream *bs;
	unsigned int checksum = flags & 0xff;
	unsigned int xattr_checksum = (flags >> 8) & 0xff;
	int atflags = 0;

	if (flags & APK_FI_NOFOLLOW)
		atflags |= AT_SYMLINK_NOFOLLOW;

	if (fstatat64(atfd, filename, &st, atflags) != 0)
		return -errno;

	*fi = (struct apk_file_info) {
		.size = st.st_size,
		.uid = st.st_uid,
		.gid = st.st_gid,
		.mode = st.st_mode,
		.mtime = st.st_mtime,
		.device = st.st_dev,
	};

	if (xattr_checksum != APK_CHECKSUM_NONE) {
		ssize_t len, vlen;
		int fd, i, r;
		char val[1024], buf[1024];

		r = 0;
		fd = openat(atfd, filename, O_RDONLY);
		if (fd >= 0) {
			len = flistxattr(fd, buf, sizeof(buf));
			if (len > 0) {
				struct apk_xattr_array *xattrs = NULL;
				apk_xattr_array_init(&xattrs);
				for (i = 0; i < len; i += strlen(&buf[i]) + 1) {
					vlen = fgetxattr(fd, &buf[i], val, sizeof(val));
					if (vlen < 0) {
						r = errno;
						if (r == ENODATA) continue;
						break;
					}
					*apk_xattr_array_add(&xattrs) = (struct apk_xattr) {
						.name = &buf[i],
						.value = *apk_blob_atomize_dup(APK_BLOB_PTR_LEN(val, vlen)),
					};
				}
				apk_fileinfo_hash_xattr_array(xattrs, apk_checksum_evp(xattr_checksum), &fi->xattr_csum);
				apk_xattr_array_free(&xattrs);
			} else if (r < 0) r = errno;
			close(fd);
		} else r = errno;

		if (r && r != ENOTSUP) return -r;
	}

	if (checksum == APK_CHECKSUM_NONE)
		return 0;

	if (S_ISDIR(st.st_mode))
		return 0;

	/* Checksum file content */
	if ((flags & APK_FI_NOFOLLOW) && S_ISLNK(st.st_mode)) {
		char *target = alloca(st.st_size);
		if (target == NULL)
			return -ENOMEM;
		if (readlinkat(atfd, filename, target, st.st_size) < 0)
			return -errno;

		EVP_Digest(target, st.st_size, fi->csum.data, NULL,
			   apk_checksum_evp(checksum), NULL);
		fi->csum.type = checksum;
	} else {
		bs = apk_bstream_from_file(atfd, filename);
		if (!IS_ERR_OR_NULL(bs)) {
			EVP_MD_CTX *mdctx;
			apk_blob_t blob;

			mdctx = EVP_MD_CTX_new();
			if (mdctx) {
				EVP_DigestInit_ex(mdctx, apk_checksum_evp(checksum), NULL);
				if (bs->flags & APK_BSTREAM_SINGLE_READ)
					EVP_MD_CTX_set_flags(mdctx, EVP_MD_CTX_FLAG_ONESHOT);
				while (!APK_BLOB_IS_NULL(blob = apk_bstream_read(bs, APK_BLOB_NULL)))
					EVP_DigestUpdate(mdctx, (void*) blob.ptr, blob.len);
				fi->csum.type = EVP_MD_CTX_size(mdctx);
				EVP_DigestFinal_ex(mdctx, fi->csum.data, NULL);
				EVP_MD_CTX_free(mdctx);
			}
			apk_bstream_close(bs, NULL);
		}
	}

	return 0;
}

void apk_fileinfo_free(struct apk_file_info *fi)
{
	apk_xattr_array_free(&fi->xattrs);
}

int apk_dir_foreach_file(int dirfd, apk_dir_file_cb cb, void *ctx)
{
	struct dirent *de;
	DIR *dir;
	int ret = 0;

	if (dirfd < 0)
		return -1;

	dir = fdopendir(dirfd);
	if (dir == NULL)
		return -1;

	/* We get called here with dup():ed fd. Since they all refer to
	 * same object, we need to rewind so subsequent calls work. */
	rewinddir(dir);

	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.') {
			if (de->d_name[1] == 0 ||
			    (de->d_name[1] == '.' && de->d_name[2] == 0))
				continue;
		}
		ret = cb(ctx, dirfd, de->d_name);
		if (ret) break;
	}
	closedir(dir);
	return ret;
}

struct apk_istream *apk_istream_from_file_gz(int atfd, const char *file)
{
	return apk_bstream_gunzip(apk_bstream_from_file(atfd, file));
}

struct apk_fd_ostream {
	struct apk_ostream os;
	int fd, rc;

	const char *file, *tmpfile;
	int atfd;

	size_t bytes;
	char buffer[1024];
};

static ssize_t safe_write(int fd, const void *ptr, size_t size)
{
	ssize_t i = 0, r;

	while (i < size) {
		r = write(fd, ptr + i, size - i);
		if (r < 0)
			return -errno;
		if (r == 0)
			return i;
		i += r;
	}

	return i;
}

static ssize_t fdo_flush(struct apk_fd_ostream *fos)
{
	ssize_t r;

	if (fos->bytes == 0)
		return 0;

	if ((r = safe_write(fos->fd, fos->buffer, fos->bytes)) != fos->bytes) {
		fos->rc = r < 0 ? r : -EIO;
		return r;
	}

	fos->bytes = 0;
	return 0;
}

static ssize_t fdo_write(void *stream, const void *ptr, size_t size)
{
	struct apk_fd_ostream *fos =
		container_of(stream, struct apk_fd_ostream, os);
	ssize_t r;

	if (size + fos->bytes >= sizeof(fos->buffer)) {
		r = fdo_flush(fos);
		if (r != 0)
			return r;
		if (size >= sizeof(fos->buffer) / 2) {
			r = safe_write(fos->fd, ptr, size);
			if (r != size)
				fos->rc = r < 0 ? r : -EIO;
			return r;
		}
	}

	memcpy(&fos->buffer[fos->bytes], ptr, size);
	fos->bytes += size;

	return size;
}

static int fdo_close(void *stream)
{
	struct apk_fd_ostream *fos =
		container_of(stream, struct apk_fd_ostream, os);
	int rc;

	fdo_flush(fos);
	rc = fos->rc;

	if (fos->fd > STDERR_FILENO &&
	    close(fos->fd) < 0)
		rc = -errno;

	if (fos->tmpfile != NULL) {
		if (rc == 0)
			renameat(fos->atfd, fos->tmpfile,
				 fos->atfd, fos->file);
		else
			unlinkat(fos->atfd, fos->tmpfile, 0);
	}

	free(fos);

	return rc;
}

static const struct apk_ostream_ops fd_ostream_ops = {
	.write = fdo_write,
	.close = fdo_close,
};

struct apk_ostream *apk_ostream_to_fd(int fd)
{
	struct apk_fd_ostream *fos;

	if (fd < 0) return ERR_PTR(-EBADF);

	fos = malloc(sizeof(struct apk_fd_ostream));
	if (fos == NULL) {
		close(fd);
		return ERR_PTR(-ENOMEM);
	}

	*fos = (struct apk_fd_ostream) {
		.os.ops = &fd_ostream_ops,
		.fd = fd,
	};

	return &fos->os;
}

struct apk_ostream *apk_ostream_to_file(int atfd,
					const char *file,
					const char *tmpfile,
					mode_t mode)
{
	struct apk_ostream *os;
	int fd;

	fd = openat(atfd, tmpfile ?: file, O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC, mode);
	if (fd < 0) return ERR_PTR(-errno);

	fcntl(fd, F_SETFD, FD_CLOEXEC);

	os = apk_ostream_to_fd(fd);
	if (IS_ERR_OR_NULL(os)) return ERR_CAST(os);

	if (tmpfile != NULL) {
		struct apk_fd_ostream *fos =
			container_of(os, struct apk_fd_ostream, os);
		fos->file = file;
		fos->tmpfile = tmpfile;
		fos->atfd = atfd;
	}
	return os;
}

struct apk_counter_ostream {
	struct apk_ostream os;
	off_t *counter;
};

static ssize_t co_write(void *stream, const void *ptr, size_t size)
{
	struct apk_counter_ostream *cos =
		container_of(stream, struct apk_counter_ostream, os);

	*cos->counter += size;
	return size;
}

static int co_close(void *stream)
{
	struct apk_counter_ostream *cos =
		container_of(stream, struct apk_counter_ostream, os);

	free(cos);
	return 0;
}

static const struct apk_ostream_ops counter_ostream_ops = {
	.write = co_write,
	.close = co_close,
};

struct apk_ostream *apk_ostream_counter(off_t *counter)
{
	struct apk_counter_ostream *cos;

	cos = malloc(sizeof(struct apk_counter_ostream));
	if (cos == NULL)
		return NULL;

	*cos = (struct apk_counter_ostream) {
		.os.ops = &counter_ostream_ops,
		.counter = counter,
	};

	return &cos->os;
}

size_t apk_ostream_write_string(struct apk_ostream *os, const char *string)
{
	size_t len;

	len = strlen(string);
	if (apk_ostream_write(os, string, len) != len)
		return -1;

	return len;
}

struct cache_item {
	apk_hash_node hash_node;
	unsigned int genid;
	union {
		uid_t uid;
		gid_t gid;
	};
	unsigned short len;
	char name[];
};

static apk_blob_t cache_item_get_key(apk_hash_item item)
{
	struct cache_item *ci = (struct cache_item *) item;
	return APK_BLOB_PTR_LEN(ci->name, ci->len);
}

static const struct apk_hash_ops id_hash_ops = {
	.node_offset = offsetof(struct cache_item, hash_node),
	.get_key = cache_item_get_key,
	.hash_key = apk_blob_hash,
	.compare = apk_blob_compare,
	.delete_item = (apk_hash_delete_f) free,
};

static struct cache_item *resolve_cache_item(struct apk_hash *hash, apk_blob_t name)
{
	struct cache_item *ci;
	unsigned long h;

	h = id_hash_ops.hash_key(name);
	ci = (struct cache_item *) apk_hash_get_hashed(hash, name, h);
	if (ci != NULL)
		return ci;

	ci = calloc(1, sizeof(struct cache_item) + name.len);
	if (ci == NULL)
		return NULL;

	ci->len = name.len;
	memcpy(ci->name, name.ptr, name.len);
	apk_hash_insert_hashed(hash, ci, h);

	return ci;
}

void apk_id_cache_init(struct apk_id_cache *idc, int root_fd)
{
	idc->root_fd = root_fd;
	idc->genid = 1;
	apk_hash_init(&idc->uid_cache, &id_hash_ops, 256);
	apk_hash_init(&idc->gid_cache, &id_hash_ops, 256);
}

void apk_id_cache_free(struct apk_id_cache *idc)
{
	apk_hash_free(&idc->uid_cache);
	apk_hash_free(&idc->gid_cache);
}

void apk_id_cache_reset(struct apk_id_cache *idc)
{
	idc->genid++;
	if (idc->genid == 0)
		idc->genid = 1;
}

uid_t apk_resolve_uid(struct apk_id_cache *idc, const char *username, uid_t default_uid)
{
#ifdef HAVE_FGETPWENT_R
	char buf[1024];
	struct passwd pwent;
#endif
	struct cache_item *ci;
	struct passwd *pwd;
	FILE *in;

	ci = resolve_cache_item(&idc->uid_cache, APK_BLOB_STR(username));
	if (ci == NULL)
		return default_uid;

	if (ci->genid != idc->genid) {
		ci->genid = idc->genid;
		ci->uid = -1;

		in = fdopen(openat(idc->root_fd, "etc/passwd", O_RDONLY|O_CLOEXEC), "r");
		if (in != NULL) {
			do {
#ifdef HAVE_FGETPWENT_R
				fgetpwent_r(in, &pwent, buf, sizeof(buf), &pwd);
#else
				pwd = fgetpwent(in);
#endif
				if (pwd == NULL)
					break;
				if (strcmp(pwd->pw_name, username) == 0) {
					ci->uid = pwd->pw_uid;
					break;
				}
			} while (1);
			fclose(in);
		}
	}

	if (ci->uid != -1)
		return ci->uid;

	return default_uid;
}

uid_t apk_resolve_gid(struct apk_id_cache *idc, const char *groupname, uid_t default_gid)
{
#ifdef HAVE_FGETGRENT_R
	char buf[1024];
	struct group grent;
#endif
	struct cache_item *ci;
	struct group *grp;
	FILE *in;

	ci = resolve_cache_item(&idc->gid_cache, APK_BLOB_STR(groupname));
	if (ci == NULL)
		return default_gid;

	if (ci->genid != idc->genid) {
		ci->genid = idc->genid;
		ci->gid = -1;

		in = fdopen(openat(idc->root_fd, "etc/group", O_RDONLY|O_CLOEXEC), "r");
		if (in != NULL) {
			do {
#ifdef HAVE_FGETGRENT_R
				fgetgrent_r(in, &grent, buf, sizeof(buf), &grp);
#else
				grp = fgetgrent(in);
#endif
				if (grp == NULL)
					break;
				if (strcmp(grp->gr_name, groupname) == 0) {
					ci->gid = grp->gr_gid;
					break;
				}
			} while (1);
			fclose(in);
		}
	}

	if (ci->gid != -1)
		return ci->gid;

	return default_gid;
}
