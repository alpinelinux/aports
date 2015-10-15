#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>

#include <sys/stat.h>
#include <sys/sendfile.h>

#define CERTSDIR "/usr/share/ca-certificates/"
#define LOCALCERTSDIR "/usr/local/share/ca-certificates/"
#define ETCCERTSDIR "/etc/ssl/certs/"
#define CERTBUNDLE "ca-certificates.crt"
#define CERTSCONF "/etc/ca-certificates.conf"

static bool str_begins(const char* str, const char* prefix)
{
	return !strncmp(str, prefix, strlen(prefix));
}

/* A string pair */
struct pair
{
	char** first;
	char** second;

	/* Total size */
	unsigned size;
	/* Fill-level */
	unsigned count;
};

static void pair_free(struct pair* data)
{
	int i = 0;
	for (i = 0; i < data->size; i++) {
		free(data->first[i]);
		free(data->second[i]);
	}
	free(data->first);
	free(data->second);
	free(data);
}

static struct pair* pair_alloc(int size)
{
       struct pair* d = (struct pair*) malloc(sizeof(struct pair));
       d->size = size;
       d->count = 0;
       d->first = calloc(size, sizeof(char** ));
       d->second = calloc(size, sizeof(char** ));
       
       return d;
}

static const char*
get_pair(struct pair* data, const char* key, int* pos)
{
	int i = 0;
	for (i = 0; i < data->size; i++) {
		*pos = i;
		if (data->second[i] && data->first[i]) {
			if (str_begins(key, data->second[i]))
				return data->first[i];
			else if (str_begins(key, data->first[i]))
				return data->second[i];
		}
	}
	
	return 0;
}

static bool
add_ca_from_pem(struct pair* data, const char* ca, const char* pem)
{
 	int count = data->count++;
	if (count >= data->size)
		return false;
        
	data->first[count] = strdup(ca);
	data->second[count] = strdup(pem);

	return true;
}

static bool
copyfile(const char* source, int output)
{
	int input;
	if ((input = open(source, O_RDONLY)) == -1)
		return -1;

	off_t bytes = 0;
	struct stat fileinfo = {0};
	fstat(input, &fileinfo);
	int result = sendfile(output, input, &bytes, fileinfo.st_size);

	close(input);

	return (fileinfo.st_size == result);
}

typedef void (*proc_path)(const char*, struct pair*, int);

static void proc_localglobaldir(const char* path, struct pair* d, int tmpfile_fd)
{
	/* basename() requires we duplicate the string */
	char* base = strdup(path);
	char* tmp_file = strdup(basename(base));
	int base_len = strlen(tmp_file);
	char* actual_file = 0;

	/* Snip off the .crt suffix*/
	if (base_len > 4 && strcmp(&tmp_file[base_len - 4], ".crt") == 0)
		tmp_file[base_len - 4] = 0;

	bool build_string = asprintf(&actual_file, "%s%s%s", "ca-cert-",
				     tmp_file, ".pem") != -1;

	if (base_len > 0 && build_string) {
		char* s;
		for (s = actual_file; *s != 0; s++) {
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

		if (add_ca_from_pem(d, path, actual_file)) {
			if (copyfile(path, tmpfile_fd) == -1)
				printf("Cant copy %s\n", path);
		} else {
			printf("Warn! Cannot add: %s\n", path);
		}
	} else {
		printf("Can't open path: %s\n", path);
	}

	free(base);
	free(actual_file);
	free(tmp_file);
}

static void proc_etccertsdir(const char* path, struct pair* d, int tmpfile_fd)
{
	struct stat statbuf;

	if (lstat(path, &statbuf) == -1)
		return;

	char* fullpath = (char*) malloc(sizeof(char*) *  statbuf.st_size + 1);
	if (readlink(path, fullpath, statbuf.st_size + 1) == -1)
		return;

	char* base = strdup(path);
	const char* actual_file = basename(base);
	int pos = -1;
	const char* target = get_pair(d, actual_file, &pos);

	if (!target) {
		/* Symlink exists but is not wanted
		 * Delete it if it points to 'our' directory
		 */
		if (str_begins(fullpath, CERTSDIR) || str_begins(fullpath, LOCALCERTSDIR))
			remove(fullpath);
	} else if (strncmp(fullpath, target, strlen(fullpath)) != 0) {
		/* Symlink exists but points wrong */
		if (symlink(target, path) == -1)
			printf("Warning! Can't link %s -> %s\n", target, path);
	} else {
		/* Symlink exists and is ok */
		memset(d->first[pos], 0, strlen(d->first[pos]));
	}

	free(base);
	free(fullpath);
}

static bool file_readline(const char* file, struct pair* d, int tmpfile_fd)
{
	FILE * fp = fopen(file, "r");
	if (fp == NULL)
		return false;

	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	while ((read = getline(&line, &len, fp)) != -1) {
		if (str_begins(line, "#") || str_begins(line, "!"))
			continue;

		char* newline = strstr(line, "\n");
		if (newline) {
			line[newline - line] = '\0';
		}

		char* fullpath = 0;
		if (asprintf(&fullpath,"%s%s", CERTSDIR, line) != -1) {
			proc_localglobaldir(fullpath, d, tmpfile_fd);
			free(fullpath);
		}
	}

	fclose(fp);
	if (line)
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

static bool dir_readfiles(struct pair* d, const char* path,
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

		int size = strlen(path) + strlen(dirp->d_name);
		char* fullpath = 0;
		if (asprintf(&fullpath, "%s%s", path, dirp->d_name) != -1) {
			if (is_filetype(fullpath, allowed_file_type))
				path_processor(fullpath, d, tmpfile_fd);

			free(fullpath);
		}
	}

	return closedir(dp) == 0;
}

int main(int a, char **v)
{
	struct pair* calinks = pair_alloc(256);

	const char* bundle = "bundleXXXXXX";
	int etccertslen = strlen(ETCCERTSDIR);
	char* tmpfile = 0;
	if (asprintf(&tmpfile, "%s%s", ETCCERTSDIR, bundle) == -1)
		return 0;

	int fd = mkstemp(tmpfile);
	if (fd == -1) {
		printf("Failed to open temporary file %s for ca bundle\n", tmpfile);
		exit(0);
	}

	/* Handle global CA certs from config file */
	file_readline(CERTSCONF, calinks, fd);

	/* Handle local CA certificates */
	dir_readfiles(calinks, LOCALCERTSDIR, FILE_REGULAR, &proc_localglobaldir, fd);

	/* Update etc cert dir for additions and deletions*/
	dir_readfiles(calinks, ETCCERTSDIR, FILE_LINK, &proc_etccertsdir, fd);

	int i = 0;
	for (i = 0; i < calinks->count; i++) {
		if (!strlen(calinks->first[i]))
			continue;
		int file_len = strlen(calinks->second[i]);
		char* newpath = 0;
		bool build_str = asprintf(&newpath, "%s%s", ETCCERTSDIR,
					 calinks->second[i]) != -1;
		if (!build_str || symlink(calinks->first[i], newpath) == -1)
			printf("Warning! Can't link %s -> %s\n",
			       calinks->first[i], newpath);
		free(newpath);
	}

	/* Update hashes and the bundle */
	if (fd != -1) {
		close(fd);
		char* newcertname = 0;
		if (asprintf(&newcertname, "%s%s", ETCCERTSDIR, CERTBUNDLE) != -1) {
			rename(tmpfile, newcertname);
			free(newcertname);
		}
	}

	pair_free(calinks);
	free(tmpfile);

	/* Execute c_rehash */
	int nullfd = open("/dev/null", O_WRONLY);
	if (nullfd == -1)
		return 0;

	if (dup2(nullfd, STDOUT_FILENO) == -1)
		return 0;

	char* c_rehash_args[] = { "/usr/bin/c_rehash", ETCCERTSDIR, ">", "/dev/null", 0 };
	execve(c_rehash_args[0], c_rehash_args, NULL);

	return 0;
}
