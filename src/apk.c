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
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>

#include <openssl/crypto.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

#include "apk_defines.h"
#include "apk_database.h"
#include "apk_applet.h"
#include "apk_blob.h"
#include "apk_print.h"

static struct apk_option generic_options[] = {
	{ 'h', "help",		"Show generic help or applet specific help" },
	{ 'p', "root",		"Install packages to DIR",
				required_argument, "DIR" },
	{ 'X', "repository",	"Use packages from REPO",
				required_argument, "REPO" },
	{ 'q', "quiet",		"Print less information" },
	{ 'v', "verbose",	"Print more information" },
	{ 'i', "interactive",	"Ask confirmation for certain operations" },
	{ 'V', "version",	"Print program version and exit" },
	{ 'f', "force",		"Do what was asked even if it looks dangerous" },
	{ 'U', "update-cache",	"Update the repository cache" },
	{ 0x101, "progress",	"Show a progress bar" },
	{ 0x102, "clean-protected", "Do not create .apk-new files to "
				"configuration dirs" },
	{ 0x106, "purge",	"Delete also modified configuration files on "
				"package removal" },
	{ 0x103, "allow-untrusted", "Blindly install packages with untrusted "
				"signatures or no signature at all" },
	{ 0x104, "simulate",	"Show what would be done without actually "
				"doing it" },
	{ 0x105, "wait",	"Wait for TIME seconds to get an exclusive "
				"repository lock before failing",
				required_argument, "TIME" },
	{ 0x107, "keys-dir",	"Override directory of trusted keys",
				required_argument, "KEYSDIR" },
	{ 0x108, "repositories-file", "Override repositories file",
				required_argument, "REPOFILE" },
	{ 0x109, "no-network",	"Do not use network (cache is still used)" },
	{ 0x111, "overlay-from-stdin", "Read list of overlay files from stdin" },
};

static int version(void)
{
	printf("apk-tools " APK_VERSION "\n");
	return 0;
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
		apk_print_indented(&indent, APK_BLOB_PTR_LEN(word, j));
	}
	if (args != NULL)
		apk_print_indented(&indent, APK_BLOB_STR(args));
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
		apk_print_indented_words(&indent, opts[i].help);
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
		apk_print_indented_words(&indent, applet->help);
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

static struct apk_repository_list *apk_repository_new(const char *url)
{
	struct apk_repository_list *r = calloc(1,
			sizeof(struct apk_repository_list));
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
	ENGINE_register_all_complete();
#endif
}

int main(int argc, char **argv)
{
	struct apk_applet *applet;
	char short_options[256], *sopt;
	struct option *opt, *all_options;
	int r, optindex, num_options;
	void *ctx = NULL;
	struct apk_repository_list *repo = NULL;
	struct apk_database db;
	struct apk_db_options dbopts;

	memset(&dbopts, 0, sizeof(dbopts));
	list_init(&dbopts.repository_list);
	umask(0);

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
		dbopts.open_flags = applet->open_flags;
		apk_flags |= applet->forced_flags;
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
			dbopts.root = optarg;
			break;
		case 0x107:
			dbopts.keys_dir = optarg;
			break;
		case 0x108:
			dbopts.repositories_file = optarg;
			break;
		case 'X':
			repo = apk_repository_new(optarg);
			if (repo)
				list_add(&repo->list, &dbopts.repository_list);
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
		case 'i':
			apk_flags |= APK_INTERACTIVE;
			break;
		case 'U':
			apk_flags |= APK_UPDATE_CACHE;
			break;
		case 0x101:
			apk_flags |= APK_PROGRESS;
			break;
		case 0x102:
			apk_flags |= APK_CLEAN_PROTECTED;
			break;
		case 0x103:
			apk_flags |= APK_ALLOW_UNTRUSTED;
			break;
		case 0x104:
			apk_flags |= APK_SIMULATE;
			break;
		case 0x106:
			apk_flags |= APK_PURGE;
			break;
		case 0x105:
			dbopts.lock_wait = atoi(optarg);
			break;
		case 0x109:
			apk_flags |= APK_NO_NETWORK;
			break;
		case 0x111:
			apk_flags |= APK_OVERLAY_FROM_STDIN;
			break;
		default:
			if (applet == NULL || applet->parse == NULL ||
			    applet->parse(ctx, &dbopts, r,
					  optindex - ARRAY_SIZE(generic_options),
					  optarg) != 0)
				return usage(applet);
			break;
		}
	}

	if (applet == NULL)
		return usage(NULL);

	argc -= optind;
	argv += optind;
	if (argc >= 1 && strcmp(argv[0], applet->name) == 0) {
		argc--;
		argv++;
	}

	r = apk_db_open(&db, &dbopts);
	if (r != 0) {
		apk_error("Failed to open apk database: %s",
			  apk_error_str(r));
		return r;
	}

	r = applet->main(ctx, &db, argc, argv);
	apk_db_close(&db);

	if (r == -EINVAL)
		return usage(applet);
	return r;
}
