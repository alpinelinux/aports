/* apk.c - Alpine Package Keeper (APK)
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
#include <fcntl.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>

#include <openssl/engine.h>

#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_blob.h"
#include "apk_csum.h"

csum_t bad_checksum = {0};
const char *apk_root;
struct apk_repository_url apk_repository_list;
int apk_verbosity = 1, apk_cwd_fd, apk_wait;
unsigned int apk_flags = 0;

static struct apk_option generic_options[] = {
	{ 'h', "help",		"Show generic help or applet specific help" },
	{ 'p', "root",		"Install packages to DIR",
				required_argument, "DIR" },
	{ 'X', "repository",	"Use packages from REPO",
				required_argument, "REPO" },
	{ 'q', "quiet",		"Print less information" },
	{ 'v', "verbose",	"Print more information" },
	{ 'V', "version",	"Print program version and exit" },
	{ 'f', "force",		"Do what was asked even if it looks dangerous" },
	{ 0x101, "progress",	"Show a progress bar" },
	{ 0x102, "clean-protected", "Do not create .apk-new files to "
				"configuration dirs" },
	{ 0x104, "simulate",	"Show what would be done without actually "
				"doing it" },
	{ 0x105, "wait",	"Wait for TIME seconds to get an exclusive "
				"repository lock before failing",
				required_argument, "TIME" },
};

void apk_log(const char *prefix, const char *format, ...)
{
	va_list va;

	if (prefix != NULL)
		fprintf(stderr, "%s", prefix);
	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
	fprintf(stderr, "\n");
}

static int version(void)
{
	printf("apk-tools " APK_VERSION "\n");
	return 0;
}

struct apk_indent {
	int x;
	int indent;
};

static int print_indented(struct apk_indent *i, apk_blob_t blob)
{
	static const int wrap_length = 80;

	if (i->x + blob.len + 1 >= wrap_length) {
		i->x = i->indent;
		printf("\n%*s", i->indent - 1, "");
	}
	i->x += printf(" %.*s", blob.len, blob.ptr);
	return 0;
}

static void print_indented_words(struct apk_indent *i, const char *text)
{
	apk_blob_for_each_segment(APK_BLOB_STR(text), " ",
		(apk_blob_cb) print_indented, i);
}

static int format_option(char *buf, size_t len, struct apk_option *o,
			 const char *separator)
{
	int i = 0;

	if (o->val <= 0xff && isalnum(o->val)) {
		i += snprintf(&buf[i], len - i, "-%c", o->val);
		if (o->name != NULL)
			i += snprintf(&buf[i], len - i, "%s", separator);
	}
	if (o->name != NULL)
		i += snprintf(&buf[i], len - i, "--%s", o->name);
	if (o->arg_name != NULL)
		i += snprintf(&buf[i], len - i, " %s", o->arg_name);

	return i;
}

static void print_usage(const char *cmd, const char *args, int num_opts,
		        struct apk_option *opts)
{
	struct apk_indent indent = { 0, 11 };
	char word[128];
	int i, j;

	indent.x = printf("\nusage: apk %s", cmd) - 1;
	for (i = 0; i < num_opts; i++) {
		j = 0;
		word[j++] = '[';
		j += format_option(&word[j], sizeof(word) - j, &opts[i], "|");
		word[j++] = ']';
		print_indented(&indent, APK_BLOB_PTR_LEN(word, j));
	}
	if (args != NULL)
		print_indented(&indent, APK_BLOB_STR(args));
	printf("\n");
}

static void print_options(int num_opts, struct apk_option *opts)
{
	struct apk_indent indent = { 0, 26 };
	char word[128];
	int i;

	for (i = 0; i < num_opts; i++) {
		format_option(word, sizeof(word), &opts[i], ", ");
		indent.x = printf("  %-*s", indent.indent - 3, word);
		print_indented_words(&indent, opts[i].help);
		printf("\n");
	}
}

static int usage(struct apk_applet *applet)
{
	struct apk_indent indent = { 0, 2 };

	version();
	if (applet == NULL) {
		struct apk_applet **a;

		print_usage("COMMAND", "[ARGS]...",
			    ARRAY_SIZE(generic_options), generic_options);

		printf("\navailable commands:\n  ");
		for (a = &__start_apkapplets; a < &__stop_apkapplets; a++)
			printf("%s ", (*a)->name);
	} else {
		print_usage(applet->name, applet->arguments,
			    applet->num_options, applet->options);
		printf("\ndescription:\n%*s", indent.indent - 1, "");
		print_indented_words(&indent, applet->help);
	}
	printf("\n\ngeneric options:\n");
	print_options(ARRAY_SIZE(generic_options), generic_options);

	if (applet != NULL && applet->num_options > 0) {
		printf("\noptions for %s command:\n", applet->name);
		print_options(applet->num_options, applet->options);
	}
	printf("\nThis apk has coffee making abilities.\n\n");

	return 1;
}

static struct apk_applet *find_applet(const char *name)
{
	struct apk_applet **a;

	for (a = &__start_apkapplets; a < &__stop_apkapplets; a++) {
		if (strcmp(name, (*a)->name) == 0)
			return *a;
	}

	return NULL;
}

static struct apk_applet *deduce_applet(int argc, char **argv)
{
	struct apk_applet *a;
	const char *prog;
	int i;

	prog = strrchr(argv[0], '/');
	if (prog == NULL)
		prog = argv[0];
	else
		prog++;

	if (strncmp(prog, "apk_", 4) == 0)
		return find_applet(prog + 4);

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-')
			continue;

		a = find_applet(argv[i]);
		if (a != NULL)
			return a;
	}

	return NULL;
}

static struct apk_repository_url *apk_repository_new(const char *url)
{
	struct apk_repository_url *r = calloc(1,
			sizeof(struct apk_repository_url));
	if (r) {
		r->url = url;
		list_init(&r->list);
	}
	return r;
}

static void merge_options(struct option *opts, struct apk_option *ao, int num)
{
	int i;

	for (i = 0; i < num; i++, opts++, ao++) {
		opts->name = ao->name;
		opts->has_arg = ao->has_arg;
		opts->flag = NULL;
		opts->val = ao->val;
	}
	opts->name = NULL;
}

static void fini_openssl(void)
{
	EVP_cleanup();
#ifndef OPENSSL_NO_ENGINE
	ENGINE_cleanup();
#endif
	CRYPTO_cleanup_all_ex_data();
}

static void init_openssl(void)
{
	atexit(fini_openssl);
	OpenSSL_add_all_algorithms();
#ifndef OPENSSL_NO_ENGINE
	ENGINE_load_builtin_engines();
#endif
}

int main(int argc, char **argv)
{
	struct apk_applet *applet;
	char short_options[256], *sopt;
	struct option *opt, *all_options;
	int r, optindex, num_options;
	void *ctx = NULL;
	struct apk_repository_url *repo = NULL;

	umask(0);
	apk_cwd_fd = open(".", O_RDONLY);
	apk_root = getenv("ROOT");
	list_init(&apk_repository_list.list);

	applet = deduce_applet(argc, argv);
	num_options = ARRAY_SIZE(generic_options) + 1;
	if (applet != NULL)
		num_options += applet->num_options;
	all_options = alloca(sizeof(struct option) * num_options);
	merge_options(&all_options[0], generic_options,
	              ARRAY_SIZE(generic_options));
	if (applet != NULL) {
		merge_options(&all_options[ARRAY_SIZE(generic_options)],
			      applet->options, applet->num_options);
		if (applet->context_size != 0)
			ctx = calloc(1, applet->context_size);
	}

	for (opt = all_options, sopt = short_options; opt->name != NULL; opt++) {
		if (opt->flag == NULL &&
		    opt->val <= 0xff && isalnum(opt->val)) {
			*(sopt++) = opt->val;
			if (opt->has_arg != no_argument)
				*(sopt++) = ':';
		}
	}

	init_openssl();

	optindex = 0;
	while ((r = getopt_long(argc, argv, short_options,
				all_options, &optindex)) != -1) {
		switch (r) {
		case 0:
			break;
		case 'h':
			return usage(applet);
			break;
		case 'p':
			apk_root = optarg;
			break;
		case 'X':
			repo = apk_repository_new(optarg);
			if (repo)
				list_add(&repo->list, &apk_repository_list.list);
			break;
		case 'q':
			apk_verbosity--;
			break;
		case 'v':
			apk_verbosity++;
			break;
		case 'V':
			return version();
		case 'f':
			apk_flags |= APK_FORCE;
			break;
		case 0x101:
			apk_flags |= APK_PROGRESS;
			break;
		case 0x102:
			apk_flags |= APK_CLEAN_PROTECTED;
			break;
		case 0x104:
			apk_flags |= APK_SIMULATE;
			break;
		case 0x105:
			apk_wait = atoi(optarg);
			break;
		default:
			if (applet == NULL || applet->parse == NULL ||
			    applet->parse(ctx, r,
					  optindex - ARRAY_SIZE(generic_options),
					  optarg) != 0)
				return usage(applet);
			break;
		}
	}

	if (applet == NULL)
		return usage(NULL);

	if (apk_root == NULL)
		apk_root = "/";

	argc -= optind;
	argv += optind;
	if (argc >= 1 && strcmp(argv[0], applet->name) == 0) {
		argc--;
		argv++;
	}

	r = applet->main(ctx, argc, argv);
	if (r == -100)
		return usage(applet);
	return r;
}
