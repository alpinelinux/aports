#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/sendfile.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define CERTSDIR "/usr/share/ca-certificates/"
#define LOCALCERTSDIR "/usr/local/share/ca-certificates/"
#define ETCCERTSDIR "/etc/ssl/certs/"
#define RUNPARTSDIR "/etc/ca-certificates/update.d/"
#define CERTBUNDLE "ca-certificates.crt"
#define CERTSCONF "/etc/ca-certificates.conf"

static const char *last_component(const char *path)
{
	const char *c = strrchr(path, '/');
	if (c) return c + 1;
	return path;
}
static bool str_begins(const char* str, const char* prefix)
{
	return !strncmp(str, prefix, strlen(prefix));
}

struct hash_item {
	struct hash_item *next;
	char *key;
	char *value;
};

struct hash {
	struct hash_item *items[256];
};

static unsigned int hash_string(const char *str)
{
	unsigned long h = 5381;
	for (; *str; str++)
		h = (h << 5) + h + *str;
	return h;
}

static void hash_init(struct hash *h)
{
	memset(h, 0, sizeof *h);
}

static struct hash_item *hash_get(struct hash *h, const char *key)
{
	unsigned int bucket = hash_string(key) % ARRAY_SIZE(h->items);
	struct hash_item *item;

	for (item = h->items[bucket]; item; item = item->next)
		if (strcmp(item->key, key) == 0)
			return item;
	return NULL;
}

static void hash_foreach(struct hash *h, void (*cb)(struct hash_item *))
{
	struct hash_item *item;
	int i;

	for (i = 0; i < ARRAY_SIZE(h->items); i++) {
		for (item = h->items[i]; item; item = item->next)
			cb(item);
	}
}

static bool hash_add(struct hash *h, const char *key, const char *value)
{
	unsigned int bucket = hash_string(key) % ARRAY_SIZE(h->items);
	size_t keylen = strlen(key), valuelen = strlen(value);
	struct hash_item *i;

	i = malloc(sizeof(struct hash_item) + keylen + 1 + valuelen + 1);
	if (!i)
		return false;

	i->key = (char*)(i+1);
	strcpy(i->key, key);
	i->value = i->key + keylen + 1;
	strcpy(i->value, value);

	i->next = h->items[bucket];
	h->items[bucket] = i;
	return true;
}

static ssize_t
buffered_copyfd(int in_fd, int out_fd, ssize_t in_size)
{
	const size_t bufsize = 8192;
	char *buf = NULL;
	ssize_t r = 0, w = 0, copied = 0, n, m;
	if ((buf = malloc(bufsize)) == NULL)
		return -1;

	while (r < in_size && (n = read(in_fd, buf, bufsize))) {
		if (n == -1) {
			if (errno == EINTR)
				continue;
			break;
		}
		r = n;
		w = 0;
		while (w < r && (n = write(out_fd, buf + w, (r - w)))) {
			if (n == -1) {
				if (errno == EINTR)
					continue;
				break;
			}
			w += n;
		}
		copied += w;
	}
	free(buf);
	return copied;
}

static bool
copyfile(const char* source, int output)
{
	off_t bytes = 0;
	struct stat fileinfo = {0};
	ssize_t result;
	int in_fd;

	if ((in_fd = open(source, O_RDONLY)) == -1)
		return false;

	if (fstat(in_fd, &fileinfo) < 0) {
		close(in_fd);
		return false;
	}

	result = sendfile(output, in_fd, &bytes, fileinfo.st_size);
	if ((result == -1) && (errno == EINVAL || errno == ENOSYS))
		result = buffered_copyfd(in_fd, output, fileinfo.st_size);

	close(in_fd);
	return fileinfo.st_size == result;
}

typedef void (*proc_path)(const char *fullpath, struct hash *, int);

static void proc_localglobaldir(const char *fullpath, struct hash *h, int tmpfile_fd)
{
	const char *fname = last_component(fullpath);
	size_t flen = strlen(fname);
	char *s, *actual_file = NULL;

	/* Snip off the .crt suffix */
	if (flen > 4 && strcmp(&fname[flen-4], ".crt") == 0)
		flen -= 4;

	if (asprintf(&actual_file, "%s%.*s%s",
				   "ca-cert-",
				   flen, fname,
				   ".pem") == -1) {
		fprintf(stderr, "Cannot open path: %s\n", fullpath);
		return;
	}

	for (s = actual_file; *s; s++) {
		switch(*s) {
		case ',':
		case ' ':
			*s = '_';
			break;
		case ')':
		case '(':
			*s = '=';
			break;
		default:
			break;
		}
	}

	if (!hash_add(h, actual_file, fullpath))
		fprintf(stderr, "Warning! Cannot hash: %s\n", fullpath);
	if (!copyfile(fullpath, tmpfile_fd))
		fprintf(stderr, "Warning! Cannot copy to bundle: %s\n", fullpath);
	free(actual_file);
}

static void proc_etccertsdir(const char* fullpath, struct hash* h, int tmpfile_fd)
{
	char linktarget[SYMLINK_MAX];
	ssize_t linklen;

	linklen = readlink(fullpath, linktarget, sizeof(linktarget)-1);
	if (linklen < 0)
		return;
	linktarget[linklen] = 0;

	struct hash_item *item = hash_get(h, last_component(fullpath));
	if (!item) {
		/* Symlink exists but is not wanted
		 * Delete it if it points to 'our' directory
		 */
		if (str_begins(linktarget, CERTSDIR) || str_begins(linktarget, LOCALCERTSDIR))
			unlink(fullpath);
	} else if (strcmp(linktarget, item->value) != 0) {
		/* Symlink exists but points wrong */
		unlink(fullpath);
		if (symlink(item->value, fullpath) < 0)
			fprintf(stderr, "Warning! Cannot update symlink %s -> %s\n", item->value, fullpath);
		item->value = 0;
	} else {
		/* Symlink exists and is ok */
		item->value = 0;
	}
}

static bool read_global_ca_list(const char* file, struct hash* d, int tmpfile_fd)
{
	FILE * fp = fopen(file, "r");
	if (fp == NULL)
		return false;

	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	while ((read = getline(&line, &len, fp)) != -1) {
		/* getline returns number of bytes in buffer, and buffer
		 * contains delimeter if it was found */
		if (read > 0 && line[read-1] == '\n')
			line[read-1] = 0;
		if (str_begins(line, "#") || str_begins(line, "!"))
			continue;

		char* fullpath = 0;
		if (asprintf(&fullpath,"%s%s", CERTSDIR, line) != -1) {
			proc_localglobaldir(fullpath, d, tmpfile_fd);
			free(fullpath);
		}
	}

	fclose(fp);
	free(line);
	return true;
}

typedef enum {
	FILE_LINK,
	FILE_REGULAR
} filetype;

static bool is_filetype(const char* path, filetype file_check)
{
	struct stat statbuf;

	if (lstat(path, &statbuf) < 0)
		return false;
	switch(file_check) {
	case FILE_LINK: return S_ISLNK(statbuf.st_mode);
	case FILE_REGULAR: return S_ISREG(statbuf.st_mode);
	default: break;
	}

	return false;
}

static bool dir_readfiles(struct hash* d, const char* path,
			  filetype allowed_file_type,
			  proc_path path_processor,
			  int tmpfile_fd)
{
	DIR *dp = opendir(path);
	if (!dp)
		return false;
 
	struct dirent *dirp;
	while ((dirp = readdir(dp)) != NULL) {
		if (str_begins(dirp->d_name, "."))
			continue;

		char* fullpath = 0;
		if (asprintf(&fullpath, "%s%s", path, dirp->d_name) != -1) {
			if (is_filetype(fullpath, allowed_file_type))
				path_processor(fullpath, d, tmpfile_fd);

			free(fullpath);
		}
	}

	return closedir(dp) == 0;
}

static void update_ca_symlink(struct hash_item *item)
{
	if (!item->value)
		return;

	char* newpath = 0;
	bool build_str = asprintf(&newpath, "%s%s", ETCCERTSDIR, item->key);
	if (!build_str || symlink(item->value, newpath) == -1)
		fprintf(stderr, "Warning! Cannot symlink %s -> %s\n",
			item->value, newpath);
	free(newpath);
}

int main(int a, char **v)
{
	struct hash _calinks, *calinks = &_calinks;

	const char* bundle = "bundleXXXXXX";
	char* tmpfile = 0;
	if (asprintf(&tmpfile, "%s%s", ETCCERTSDIR, bundle) == -1)
		return 1;

	int fd = mkstemp(tmpfile);
	if (fd == -1) {
		fprintf(stderr, "Failed to open temporary file %s for ca bundle\n", tmpfile);
		return 1;
	}
	fchmod(fd, 0644);

	hash_init(calinks);

	/* Handle global CA certs from config file */
	read_global_ca_list(CERTSCONF, calinks, fd);

	/* Handle local CA certificates */
	dir_readfiles(calinks, LOCALCERTSDIR, FILE_REGULAR, &proc_localglobaldir, fd);

	/* Update etc cert dir for additions and deletions*/
	dir_readfiles(calinks, ETCCERTSDIR, FILE_LINK, &proc_etccertsdir, fd);
	hash_foreach(calinks, update_ca_symlink);

	/* Update hashes and the bundle */
	if (fd != -1) {
		close(fd);
		char* newcertname = 0;
		if (asprintf(&newcertname, "%s%s", ETCCERTSDIR, CERTBUNDLE) != -1) {
			rename(tmpfile, newcertname);
			free(newcertname);
		}
	}

	free(tmpfile);

	/* Execute run-parts */
	static const char *run_parts_args[] = { "run-parts", RUNPARTSDIR, 0 };
	execve("/usr/bin/run-parts", run_parts_args, NULL);
	execve("/bin/run-parts", run_parts_args, NULL);
	perror("run-parts");

	return 1;
}
