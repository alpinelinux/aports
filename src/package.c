/* package.c - Alpine Package Keeper (APK)
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
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "apk_openssl.h"
#include <openssl/pem.h>

#include "apk_defines.h"
#include "apk_archive.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_print.h"

static const apk_spn_match_def apk_spn_dependency_comparer = {
	[7] = (1<<4) /*<*/ | (1<<5) /*=*/ | (1<<6) /*<*/,
	[15] = (1<<6) /*~*/
};

static const apk_spn_match_def apk_spn_dependency_separator = {
	[1] = (1<<2) /*\n*/,
	[4] = (1<<0) /* */,
};

static const apk_spn_match_def apk_spn_repotag_separator = {
	[8] = (1<<0) /*@*/
};

struct apk_package *apk_pkg_get_installed(struct apk_name *name)
{
	struct apk_provider *p;

	foreach_array_item(p, name->providers)
		if (p->pkg->name == name && p->pkg->ipkg != NULL)
			return p->pkg;

	return NULL;
}

struct apk_package *apk_pkg_new(void)
{
	struct apk_package *pkg;

	pkg = calloc(1, sizeof(struct apk_package));
	if (pkg != NULL) {
		apk_dependency_array_init(&pkg->depends);
		apk_dependency_array_init(&pkg->install_if);
		apk_dependency_array_init(&pkg->provides);
	}

	return pkg;
}

struct apk_installed_package *apk_pkg_install(struct apk_database *db,
					      struct apk_package *pkg)
{
	struct apk_installed_package *ipkg;

	if (pkg->ipkg != NULL)
		return pkg->ipkg;

	pkg->ipkg = ipkg = calloc(1, sizeof(struct apk_installed_package));
	ipkg->pkg = pkg;
	apk_string_array_init(&ipkg->triggers);
	apk_string_array_init(&ipkg->pending_triggers);
	apk_dependency_array_init(&ipkg->replaces);

	/* Overlay override information resides in a nameless package */
	if (pkg->name != NULL) {
		db->installed.stats.packages++;
		db->installed.stats.bytes += pkg->installed_size;
		list_add_tail(&ipkg->installed_pkgs_list,
			      &db->installed.packages);
	}

	return ipkg;
}

void apk_pkg_uninstall(struct apk_database *db, struct apk_package *pkg)
{
	struct apk_installed_package *ipkg = pkg->ipkg;
	char **trigger;
	int i;

	if (ipkg == NULL)
		return;

	if (db != NULL) {
		db->installed.stats.packages--;
		db->installed.stats.bytes -= pkg->installed_size;
	}

	list_del(&ipkg->installed_pkgs_list);

	if (ipkg->triggers->num) {
		list_del(&ipkg->trigger_pkgs_list);
		list_init(&ipkg->trigger_pkgs_list);
		foreach_array_item(trigger, ipkg->triggers)
			free(*trigger);
	}
	apk_string_array_free(&ipkg->triggers);
	apk_string_array_free(&ipkg->pending_triggers);
	apk_dependency_array_free(&ipkg->replaces);

	for (i = 0; i < APK_SCRIPT_MAX; i++)
		if (ipkg->script[i].ptr != NULL)
			free(ipkg->script[i].ptr);
	free(ipkg);
	pkg->ipkg = NULL;
}

int apk_pkg_parse_name(apk_blob_t apkname,
		       apk_blob_t *name,
		       apk_blob_t *version)
{
	int i, dash = 0;

	if (APK_BLOB_IS_NULL(apkname))
		return -1;

	for (i = apkname.len - 2; i >= 0; i--) {
		if (apkname.ptr[i] != '-')
			continue;
		if (isdigit(apkname.ptr[i+1]))
			break;
		if (++dash >= 2)
			return -1;
	}
	if (i < 0)
		return -1;

	if (name != NULL)
		*name = APK_BLOB_PTR_LEN(apkname.ptr, i);
	if (version != NULL)
		*version = APK_BLOB_PTR_PTR(&apkname.ptr[i+1],
					    &apkname.ptr[apkname.len-1]);

	return 0;
}

void apk_deps_add(struct apk_dependency_array **depends, struct apk_dependency *dep)
{
	struct apk_dependency *d0;

	if (*depends) {
		foreach_array_item(d0, *depends) {
			if (d0->name == dep->name) {
				*d0 = *dep;
				return;
			}
		}
	}
	*apk_dependency_array_add(depends) = *dep;
}

void apk_deps_del(struct apk_dependency_array **pdeps, struct apk_name *name)
{
	struct apk_dependency_array *deps = *pdeps;
	struct apk_dependency *d0;

	if (deps == NULL)
		return;

	foreach_array_item(d0, deps) {
		if (d0->name == name) {
			*d0 = deps->item[deps->num - 1];
			apk_dependency_array_resize(pdeps, deps->num - 1);
			break;
		}
	}
}

void apk_blob_pull_dep(apk_blob_t *b, struct apk_database *db, struct apk_dependency *dep)
{
	struct apk_name *name;
	apk_blob_t bdep, bname, bop, bver = APK_BLOB_NULL, btag;
	int mask = APK_DEPMASK_ANY, conflict = 0, tag = 0, fuzzy = 0;

	/* [!]name[<,<=,<~,=,~,>~,>=,>,><]ver */
	if (APK_BLOB_IS_NULL(*b))
		goto fail;

	/* grap one token */
	if (!apk_blob_cspn(*b, apk_spn_dependency_separator, &bdep, NULL))
		bdep = *b;
	b->ptr += bdep.len;
	b->len -= bdep.len;

	/* skip also all separator chars */
	if (!apk_blob_spn(*b, apk_spn_dependency_separator, NULL, b)) {
		b->ptr += b->len;
		b->len = 0;
	}

	/* parse the version */
	if (bdep.ptr[0] == '!') {
		bdep.ptr++;
		bdep.len--;
		conflict = 1;
	}

	if (apk_blob_cspn(bdep, apk_spn_dependency_comparer, &bname, &bop)) {
		int i;

		if (mask == 0)
			goto fail;
		if (!apk_blob_spn(bop, apk_spn_dependency_comparer, &bop, &bver))
			goto fail;
		mask = 0;
		for (i = 0; i < bop.len; i++) {
			switch (bop.ptr[i]) {
			case '<':
				mask |= APK_VERSION_LESS;
				break;
			case '>':
				mask |= APK_VERSION_GREATER;
				break;
			case '~':
				mask |= APK_VERSION_FUZZY|APK_VERSION_EQUAL;
				fuzzy = TRUE;
				break;
			case '=':
				mask |= APK_VERSION_EQUAL;
				break;
			}
		}
		if ((mask & APK_DEPMASK_CHECKSUM) != APK_DEPMASK_CHECKSUM &&
		    !apk_version_validate(bver))
			goto fail;
	} else {
		bname = bdep;
		bop = APK_BLOB_NULL;
		bver = APK_BLOB_NULL;
	}

	if (apk_blob_cspn(bname, apk_spn_repotag_separator, &bname, &btag))
		tag = apk_db_get_tag_id(db, btag);

	/* convert to apk_dependency */
	name = apk_db_get_name(db, bname);
	if (name == NULL)
		goto fail;

	*dep = (struct apk_dependency){
		.name = name,
		.version = apk_blob_atomize_dup(bver),
		.repository_tag = tag,
		.result_mask = mask,
		.conflict = conflict,
		.fuzzy = fuzzy,
	};
	return;
fail:
	*dep = (struct apk_dependency){ .name = NULL };
	*b = APK_BLOB_NULL;
}

void apk_blob_pull_deps(apk_blob_t *b, struct apk_database *db, struct apk_dependency_array **deps)
{
	struct apk_dependency dep;

	while (b->len > 0) {
		apk_blob_pull_dep(b, db, &dep);
		if (APK_BLOB_IS_NULL(*b) || dep.name == NULL)
			break;

		*apk_dependency_array_add(deps) = dep;
	}
}

void apk_dep_from_pkg(struct apk_dependency *dep, struct apk_database *db,
		      struct apk_package *pkg)
{
	char buf[64];
	apk_blob_t b = APK_BLOB_BUF(buf);

	apk_blob_push_csum(&b, &pkg->csum);
	b = apk_blob_pushed(APK_BLOB_BUF(buf), b);

	*dep = (struct apk_dependency) {
		.name = pkg->name,
		.version = apk_blob_atomize_dup(b),
		.result_mask = APK_DEPMASK_CHECKSUM,
	};
}

static int apk_dep_match_checksum(struct apk_dependency *dep, struct apk_package *pkg)
{
	struct apk_checksum csum;
	apk_blob_t b = *dep->version;

	apk_blob_pull_csum(&b, &csum);
	if (apk_checksum_compare(&csum, &pkg->csum) == 0)
		return 1;

	return 0;
}

int apk_dep_is_provided(struct apk_dependency *dep, struct apk_provider *p)
{
	if (p == NULL || p->pkg == NULL)
		return dep->conflict;

	switch (dep->result_mask) {
	case APK_DEPMASK_CHECKSUM:
		return apk_dep_match_checksum(dep, p->pkg);
	case APK_DEPMASK_ANY:
		return !dep->conflict;
	default:
		if (p->version == &apk_null_blob)
			return dep->conflict;
		if (apk_version_compare_blob_fuzzy(*p->version, *dep->version, dep->fuzzy)
		    & dep->result_mask)
			return !dep->conflict;
		return dep->conflict;
	}
	return dep->conflict;
}

int apk_dep_is_materialized(struct apk_dependency *dep, struct apk_package *pkg)
{
	if (pkg == NULL)
		return dep->conflict;
	if (dep->name != pkg->name)
		return dep->conflict;

	switch (dep->result_mask) {
	case APK_DEPMASK_CHECKSUM:
		return apk_dep_match_checksum(dep, pkg);
	case APK_DEPMASK_ANY:
		return !dep->conflict;
	default:
		if (apk_version_compare_blob_fuzzy(*pkg->version, *dep->version, dep->fuzzy)
		    & dep->result_mask)
			return !dep->conflict;
		return dep->conflict;
	}
	return dep->conflict;
}

int apk_dep_analyze(struct apk_dependency *dep, struct apk_package *pkg)
{
	struct apk_dependency *p;
	struct apk_provider provider;

	if (pkg == NULL)
		return APK_DEP_IRRELEVANT;

	if (dep->name == pkg->name)
		return apk_dep_is_materialized(dep, pkg) ? APK_DEP_SATISFIES : APK_DEP_CONFLICTS;

	foreach_array_item(p, pkg->provides) {
		if (p->name != dep->name)
			continue;
		provider = APK_PROVIDER_FROM_PROVIDES(pkg, p);
		return apk_dep_is_provided(dep, &provider) ? APK_DEP_SATISFIES : APK_DEP_CONFLICTS;
	}

	return APK_DEP_IRRELEVANT;
}

char *apk_dep_snprintf(char *buf, size_t n, struct apk_dependency *dep)
{
	apk_blob_t b = APK_BLOB_PTR_LEN(buf, n);
	apk_blob_push_dep(&b, NULL, dep);
	if (b.len)
		apk_blob_push_blob(&b, APK_BLOB_PTR_LEN("", 1));
	else
		b.ptr[-1] = 0;
	return buf;
}

void apk_blob_push_dep(apk_blob_t *to, struct apk_database *db, struct apk_dependency *dep)
{
	int result_mask = dep->result_mask;

	if (dep->conflict)
		apk_blob_push_blob(to, APK_BLOB_PTR_LEN("!", 1));

	apk_blob_push_blob(to, APK_BLOB_STR(dep->name->name));
	if (dep->repository_tag && db != NULL)
		apk_blob_push_blob(to, db->repo_tags[dep->repository_tag].tag);
	if (!APK_BLOB_IS_NULL(*dep->version)) {
		apk_blob_push_blob(to, APK_BLOB_STR(apk_version_op_string(result_mask)));
		apk_blob_push_blob(to, *dep->version);
	}
}

void apk_blob_push_deps(apk_blob_t *to, struct apk_database *db, struct apk_dependency_array *deps)
{
	int i;

	if (deps == NULL)
		return;

	for (i = 0; i < deps->num; i++) {
		if (i)
			apk_blob_push_blob(to, APK_BLOB_PTR_LEN(" ", 1));
		apk_blob_push_dep(to, db, &deps->item[i]);
	}
}

int apk_deps_write(struct apk_database *db, struct apk_dependency_array *deps, struct apk_ostream *os, apk_blob_t separator)
{
	apk_blob_t blob;
	char tmp[256];
	int i, n = 0;

	if (deps == NULL)
		return 0;

	for (i = 0; i < deps->num; i++) {
		blob = APK_BLOB_BUF(tmp);
		if (i)
			apk_blob_push_blob(&blob, separator);
		apk_blob_push_dep(&blob, db, &deps->item[i]);

		blob = apk_blob_pushed(APK_BLOB_BUF(tmp), blob);
		if (APK_BLOB_IS_NULL(blob) || 
		    apk_ostream_write(os, blob.ptr, blob.len) != blob.len)
			return -1;

		n += blob.len;
	}

	return n;
}

const char *apk_script_types[] = {
	[APK_SCRIPT_PRE_INSTALL]	= "pre-install",
	[APK_SCRIPT_POST_INSTALL]	= "post-install",
	[APK_SCRIPT_PRE_DEINSTALL]	= "pre-deinstall",
	[APK_SCRIPT_POST_DEINSTALL]	= "post-deinstall",
	[APK_SCRIPT_PRE_UPGRADE]	= "pre-upgrade",
	[APK_SCRIPT_POST_UPGRADE]	= "post-upgrade",
	[APK_SCRIPT_TRIGGER]		= "trigger",
};

int apk_script_type(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(apk_script_types); i++)
		if (apk_script_types[i] &&
		    strcmp(apk_script_types[i], name) == 0)
			return i;

	return APK_SCRIPT_INVALID;
}

void apk_sign_ctx_init(struct apk_sign_ctx *ctx, int action,
		       struct apk_checksum *identity, int keys_fd)
{
	memset(ctx, 0, sizeof(struct apk_sign_ctx));
	ctx->keys_fd = keys_fd;
	ctx->action = action;
	switch (action) {
	case APK_SIGN_VERIFY:
		ctx->md = EVP_md_null();
		break;
	case APK_SIGN_VERIFY_IDENTITY:
		ctx->md = EVP_sha1();
		memcpy(&ctx->identity, identity, sizeof(ctx->identity));
		break;
	case APK_SIGN_GENERATE:
	case APK_SIGN_VERIFY_AND_GENERATE:
		ctx->md = EVP_sha1();
		break;
	default:
		action = APK_SIGN_NONE;
		ctx->md = EVP_md_null();
		ctx->control_started = 1;
		ctx->data_started = 1;
		break;
	}
	ctx->mdctx = EVP_MD_CTX_new();
	EVP_DigestInit_ex(ctx->mdctx, ctx->md, NULL);
	EVP_MD_CTX_set_flags(ctx->mdctx, EVP_MD_CTX_FLAG_ONESHOT);
}

void apk_sign_ctx_free(struct apk_sign_ctx *ctx)
{
	if (ctx->signature.data.ptr != NULL)
		free(ctx->signature.data.ptr);
	if (ctx->signature.pkey != NULL)
		EVP_PKEY_free(ctx->signature.pkey);
	EVP_MD_CTX_free(ctx->mdctx);
}

static int check_signing_key_trust(struct apk_sign_ctx *sctx)
{
	switch (sctx->action) {
	case APK_SIGN_VERIFY:
	case APK_SIGN_VERIFY_AND_GENERATE:
		if (sctx->signature.pkey == NULL) {
			if (apk_flags & APK_ALLOW_UNTRUSTED)
				break;
			return -ENOKEY;
		}
	}
	return 0;
}

int apk_sign_ctx_process_file(struct apk_sign_ctx *ctx,
			      const struct apk_file_info *fi,
			      struct apk_istream *is)
{
	static struct {
		char type[8];
		unsigned int nid;
	} signature_type[] = {
		{ "RSA512", NID_sha512 },
		{ "RSA256", NID_sha256 },
		{ "RSA", NID_sha1 },
		{ "DSA", NID_dsa },
	};
	const EVP_MD *md = NULL;
	const char *name = NULL;
	BIO *bio;
	int r, i, fd;

	if (ctx->data_started)
		return 1;

	if (fi->name[0] != '.' || strchr(fi->name, '/') != NULL) {
		/* APKv1.0 compatibility - first non-hidden file is
		 * considered to start the data section of the file.
		 * This does not make any sense if the file has v2.0
		 * style .PKGINFO */
		if (ctx->has_data_checksum)
			return -ENOMSG;
		/* Error out early if identity part is missing */
		if (ctx->action == APK_SIGN_VERIFY_IDENTITY)
			return -EKEYREJECTED;
		ctx->data_started = 1;
		ctx->control_started = 1;
		r = check_signing_key_trust(ctx);
		if (r < 0)
			return r;
		return 1;
	}

	if (ctx->control_started)
		return 1;

	if (strncmp(fi->name, ".SIGN.", 6) != 0) {
		ctx->control_started = 1;
		return 1;
	}

	/* A signature file */
	ctx->num_signatures++;

	/* Found already a trusted key */
	if ((ctx->action != APK_SIGN_VERIFY &&
	     ctx->action != APK_SIGN_VERIFY_AND_GENERATE) ||
	    ctx->signature.pkey != NULL)
		return 0;

	if (ctx->keys_fd < 0)
		return 0;

	for (i = 0; i < ARRAY_SIZE(signature_type); i++) {
		size_t slen = strlen(signature_type[i].type);
		if (strncmp(&fi->name[6], signature_type[i].type, slen) == 0 &&
		    fi->name[6+slen] == '.') {
			md = EVP_get_digestbynid(signature_type[i].nid);
			name = &fi->name[6+slen+1];
			break;
		}
	}
	if (!md) return 0;

	fd = openat(ctx->keys_fd, name, O_RDONLY|O_CLOEXEC);
	if (fd < 0) return 0;

	bio = BIO_new_fp(fdopen(fd, "r"), BIO_CLOSE);
	ctx->signature.pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
	if (ctx->signature.pkey != NULL) {
		ctx->md = md;
		ctx->signature.data = apk_blob_from_istream(is, fi->size);
	}
	BIO_free(bio);

	return 0;
}

int apk_sign_ctx_parse_pkginfo_line(void *ctx, apk_blob_t line)
{
	struct apk_sign_ctx *sctx = (struct apk_sign_ctx *) ctx;
	apk_blob_t l, r;

	if (line.ptr == NULL || line.len < 1 || line.ptr[0] == '#')
		return 0;

	if (!apk_blob_split(line, APK_BLOB_STR(" = "), &l, &r))
		return 0;

	if (sctx->data_started == 0 &&
	    apk_blob_compare(APK_BLOB_STR("datahash"), l) == 0) {
		sctx->has_data_checksum = 1;
		sctx->md = EVP_sha256();
		apk_blob_pull_hexdump(
			&r, APK_BLOB_PTR_LEN(sctx->data_checksum,
					     EVP_MD_size(sctx->md)));
	}

	return 0;
}

int apk_sign_ctx_verify_tar(void *sctx, const struct apk_file_info *fi,
			    struct apk_istream *is)
{
	struct apk_sign_ctx *ctx = (struct apk_sign_ctx *) sctx;
	int r;

	r = apk_sign_ctx_process_file(ctx, fi, is);
	if (r <= 0)
		return r;

	if (strcmp(fi->name, ".PKGINFO") == 0) {
		apk_blob_t blob = apk_blob_from_istream(is, fi->size);
		apk_blob_for_each_segment(
			blob, "\n",
			apk_sign_ctx_parse_pkginfo_line, ctx);
		free(blob.ptr);
	}

	return 0;
}

int apk_sign_ctx_mpart_cb(void *ctx, int part, apk_blob_t data)
{
	struct apk_sign_ctx *sctx = (struct apk_sign_ctx *) ctx;
	unsigned char calculated[EVP_MAX_MD_SIZE];
	int r, end_of_control;

	if ((part == APK_MPART_DATA) ||
	    (part == APK_MPART_BOUNDARY && sctx->data_started))
		goto update_digest;

	/* Still in signature blocks? */
	if (!sctx->control_started) {
		if (part == APK_MPART_END)
			return -EKEYREJECTED;
		goto reset_digest;
	}

	/* Grab state and mark all remaining block as data */
	end_of_control = (sctx->data_started == 0);
	sctx->data_started = 1;

	/* End of control-block and control does not have data checksum? */
	if (sctx->has_data_checksum == 0 && end_of_control &&
	    part != APK_MPART_END)
		goto update_digest;

	/* Drool in the remaining of the digest block now, we will finish
	 * it on all cases */
	EVP_DigestUpdate(sctx->mdctx, data.ptr, data.len);

	/* End of control-block and checking control hash/signature or
	 * end of data-block and checking its hash/signature */
	if (sctx->has_data_checksum && !end_of_control) {
		/* End of control-block and check it's hash */
		EVP_DigestFinal_ex(sctx->mdctx, calculated, NULL);
		if (EVP_MD_CTX_size(sctx->mdctx) == 0 ||
		    memcmp(calculated, sctx->data_checksum,
		           EVP_MD_CTX_size(sctx->mdctx)) != 0)
			return -EKEYREJECTED;
		sctx->data_verified = 1;
		if (!(apk_flags & APK_ALLOW_UNTRUSTED) &&
		    !sctx->control_verified)
			return -ENOKEY;
		return 0;
	}

	r = check_signing_key_trust(sctx);
	if (r < 0)
		return r;

	switch (sctx->action) {
	case APK_SIGN_VERIFY:
	case APK_SIGN_VERIFY_AND_GENERATE:
		if (sctx->signature.pkey != NULL) {
			r = EVP_VerifyFinal(sctx->mdctx,
				(unsigned char *) sctx->signature.data.ptr,
				sctx->signature.data.len,
				sctx->signature.pkey);
			if (r != 1 && !(apk_flags & APK_ALLOW_UNTRUSTED))
				return -EKEYREJECTED;
		} else {
			r = 0;
			if (!(apk_flags & APK_ALLOW_UNTRUSTED))
				return -ENOKEY;
		}
		if (r == 1) {
			sctx->control_verified = 1;
			if (!sctx->has_data_checksum && part == APK_MPART_END)
				sctx->data_verified = 1;
		}
		if (sctx->action == APK_SIGN_VERIFY_AND_GENERATE) {
			sctx->identity.type = EVP_MD_CTX_size(sctx->mdctx);
			EVP_DigestFinal_ex(sctx->mdctx, sctx->identity.data, NULL);
		}
		break;
	case APK_SIGN_VERIFY_IDENTITY:
		/* Reset digest for hashing data */
		EVP_DigestFinal_ex(sctx->mdctx, calculated, NULL);
		if (memcmp(calculated, sctx->identity.data,
			   sctx->identity.type) != 0)
			return -EKEYREJECTED;
		sctx->control_verified = 1;
		if (!sctx->has_data_checksum && part == APK_MPART_END)
			sctx->data_verified = 1;
		break;
	case APK_SIGN_GENERATE:
		/* Package identity is the checksum */
		sctx->identity.type = EVP_MD_CTX_size(sctx->mdctx);
		EVP_DigestFinal_ex(sctx->mdctx, sctx->identity.data, NULL);
		if (sctx->action == APK_SIGN_GENERATE &&
		    sctx->has_data_checksum)
			return -ECANCELED;
		break;
	}
reset_digest:
	EVP_DigestInit_ex(sctx->mdctx, sctx->md, NULL);
	EVP_MD_CTX_set_flags(sctx->mdctx, EVP_MD_CTX_FLAG_ONESHOT);
	return 0;

update_digest:
	EVP_MD_CTX_clear_flags(sctx->mdctx, EVP_MD_CTX_FLAG_ONESHOT);
	EVP_DigestUpdate(sctx->mdctx, data.ptr, data.len);
	return 0;
}

struct read_info_ctx {
	struct apk_database *db;
	struct apk_package *pkg;
	struct apk_sign_ctx *sctx;
};

int apk_pkg_add_info(struct apk_database *db, struct apk_package *pkg,
		     char field, apk_blob_t value)
{
	switch (field) {
	case 'P':
		pkg->name = apk_db_get_name(db, value);
		break;
	case 'V':
		pkg->version = apk_blob_atomize_dup(value);
		break;
	case 'T':
		pkg->description = apk_blob_cstr(value);
		break;
	case 'U':
		pkg->url = apk_blob_cstr(value);
		break;
	case 'L':
		pkg->license = apk_blob_atomize_dup(value);
		break;
	case 'A':
		pkg->arch = apk_blob_atomize_dup(value);
		break;
	case 'D':
		apk_blob_pull_deps(&value, db, &pkg->depends);
		break;
	case 'C':
		apk_blob_pull_csum(&value, &pkg->csum);
		break;
	case 'S':
		pkg->size = apk_blob_pull_uint(&value, 10);
		break;
	case 'I':
		pkg->installed_size = apk_blob_pull_uint(&value, 10);
		break;
	case 'p':
		apk_blob_pull_deps(&value, db, &pkg->provides);
		break;
	case 'i':
		apk_blob_pull_deps(&value, db, &pkg->install_if);
		break;
	case 'o':
		pkg->origin = apk_blob_atomize_dup(value);
		break;
	case 'm':
		pkg->maintainer = apk_blob_atomize_dup(value);
		break;
	case 't':
		pkg->build_time = apk_blob_pull_uint(&value, 10);
		break;
	case 'c':
		pkg->commit = apk_blob_cstr(value);
		break;
	case 'k':
		pkg->provider_priority = apk_blob_pull_uint(&value, 10);
		break;
	case 'F': case 'M': case 'R': case 'Z': case 'r': case 'q':
	case 'a': case 's': case 'f':
		/* installed db entries which are handled in database.c */
		return 1;
	default:
		/* lower case index entries are safe to be ignored */
		if (!islower(field)) {
			pkg->uninstallable = 1;
			db->compat_notinstallable = 1;
		}
		db->compat_newfeatures = 1;
		return 2;
	}
	if (APK_BLOB_IS_NULL(value))
		return -1;
	return 0;
}

static int read_info_line(void *ctx, apk_blob_t line)
{
	static struct {
		const char *str;
		char field;
	} fields[] = {
		{ "pkgname",	'P' },
		{ "pkgver", 	'V' },
		{ "pkgdesc",	'T' },
		{ "url",	'U' },
		{ "size",	'I' },
		{ "license",	'L' },
		{ "arch",	'A' },
		{ "depend",	'D' },
		{ "install_if",	'i' },
		{ "provides",	'p' },
		{ "origin",	'o' },
		{ "maintainer",	'm' },
		{ "builddate",	't' },
		{ "commit",	'c' },
		{ "provider_priority", 'k' },
	};
	struct read_info_ctx *ri = (struct read_info_ctx *) ctx;
	apk_blob_t l, r;
	int i;

	if (line.ptr == NULL || line.len < 1 || line.ptr[0] == '#')
		return 0;

	if (!apk_blob_split(line, APK_BLOB_STR(" = "), &l, &r))
		return 0;

	for (i = 0; i < ARRAY_SIZE(fields); i++) {
		if (apk_blob_compare(APK_BLOB_STR(fields[i].str), l) == 0) {
			apk_pkg_add_info(ri->db, ri->pkg, fields[i].field, r);
			return 0;
		}
	}
	apk_sign_ctx_parse_pkginfo_line(ri->sctx, line);

	return 0;
}

static int read_info_entry(void *ctx, const struct apk_file_info *ae,
			   struct apk_istream *is)
{
	struct read_info_ctx *ri = (struct read_info_ctx *) ctx;
	struct apk_package *pkg = ri->pkg;
	int r;

	/* Meta info and scripts */
	r = apk_sign_ctx_process_file(ri->sctx, ae, is);
	if (r <= 0)
		return r;

	if (ae->name[0] == '.') {
		/* APK 2.0 format */
		if (strcmp(ae->name, ".PKGINFO") == 0) {
			apk_blob_t blob = apk_blob_from_istream(is, ae->size);
			apk_blob_for_each_segment(blob, "\n", read_info_line, ctx);
			free(blob.ptr);
		} else if (strcmp(ae->name, ".INSTALL") == 0) {
			apk_warning("Package '%s-%s' contains deprecated .INSTALL",
				    pkg->name->name, pkg->version);
		}
		return 0;
	}

	return 0;
}

int apk_pkg_read(struct apk_database *db, const char *file,
	         struct apk_sign_ctx *sctx, struct apk_package **pkg)
{
	struct read_info_ctx ctx;
	struct apk_file_info fi;
	struct apk_bstream *bs;
	struct apk_istream *tar;
	int r;

	r = apk_fileinfo_get(AT_FDCWD, file, APK_CHECKSUM_NONE, &fi);
	if (r != 0)
		return r;

	memset(&ctx, 0, sizeof(ctx));
	ctx.sctx = sctx;
	ctx.pkg = apk_pkg_new();
	r = -ENOMEM;
	if (ctx.pkg == NULL)
		goto err;
	bs = apk_bstream_from_file(AT_FDCWD, file);
	if (IS_ERR_OR_NULL(bs)) {
		r = PTR_ERR(bs) ?: -EIO;
		goto err;
	}

	ctx.db = db;
	ctx.pkg->size = fi.size;

	tar = apk_bstream_gunzip_mpart(bs, apk_sign_ctx_mpart_cb, sctx);
	r = apk_tar_parse(tar, read_info_entry, &ctx, FALSE, &db->id_cache);
	apk_istream_close(tar);
	if (r < 0 && r != -ECANCELED)
		goto err;
	if (ctx.pkg->name == NULL || ctx.pkg->uninstallable) {
		r = -ENOTSUP;
		goto err;
	}
	if (sctx->action != APK_SIGN_VERIFY)
		ctx.pkg->csum = sctx->identity;
	ctx.pkg->filename = strdup(file);

	ctx.pkg = apk_db_pkg_add(db, ctx.pkg);
	if (pkg != NULL)
		*pkg = ctx.pkg;
	return 0;
err:
	apk_pkg_free(ctx.pkg);
	return r;
}

void apk_pkg_free(struct apk_package *pkg)
{
	if (pkg == NULL)
		return;

	apk_pkg_uninstall(NULL, pkg);
	apk_dependency_array_free(&pkg->depends);
	apk_dependency_array_free(&pkg->provides);
	apk_dependency_array_free(&pkg->install_if);
	if (pkg->url)
		free(pkg->url);
	if (pkg->description)
		free(pkg->description);
	if (pkg->commit)
		free(pkg->commit);
	free(pkg);
}

int apk_ipkg_add_script(struct apk_installed_package *ipkg,
			struct apk_istream *is,
			unsigned int type, unsigned int size)
{
	void *ptr;
	int r;

	if (type >= APK_SCRIPT_MAX)
		return -1;

	ptr = malloc(size);
	r = apk_istream_read(is, ptr, size);
	if (r < 0) {
		free(ptr);
		return r;
	}

	if (ipkg->script[type].ptr)
		free(ipkg->script[type].ptr);
	ipkg->script[type].ptr = ptr;
	ipkg->script[type].len = size;
	return 0;
}

void apk_ipkg_run_script(struct apk_installed_package *ipkg,
			 struct apk_database *db,
			 unsigned int type, char **argv)
{
	struct apk_package *pkg = ipkg->pkg;
	char fn[PATH_MAX];
	int fd, root_fd = db->root_fd;

	if (type >= APK_SCRIPT_MAX || ipkg->script[type].ptr == NULL)
		return;

	argv[0] = (char *) apk_script_types[type];

	/* Avoid /tmp as it can be mounted noexec */
	snprintf(fn, sizeof(fn), "var/cache/misc/" PKG_VER_FMT ".%s",
		PKG_VER_PRINTF(pkg),
		apk_script_types[type]);

	if ((apk_flags & (APK_NO_SCRIPTS | APK_SIMULATE)) != 0)
		return;

	apk_message("Executing %s", &fn[15]);
	fd = openat(root_fd, fn, O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC, 0755);
	if (fd < 0) {
		mkdirat(root_fd, "var/cache/misc", 0755);
		fd = openat(root_fd, fn, O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC, 0755);
		if (fd < 0) goto err_log;
	}
	if (write(fd, ipkg->script[type].ptr, ipkg->script[type].len) < 0) {
		close(fd);
		goto err_log;
	}
	close(fd);

	if (apk_db_run_script(db, fn, argv) < 0)
		goto err;

	/* Script may have done something that changes id cache contents */
	apk_id_cache_reset(&db->id_cache);

	goto cleanup;

err_log:
	apk_error("%s: failed to execute: %s", &fn[15], apk_error_str(errno));
err:
	ipkg->broken_script = 1;
cleanup:
	unlinkat(root_fd, fn, 0);
}

static int parse_index_line(void *ctx, apk_blob_t line)
{
	struct read_info_ctx *ri = (struct read_info_ctx *) ctx;

	if (line.len < 3 || line.ptr[1] != ':')
		return 0;

	apk_pkg_add_info(ri->db, ri->pkg, line.ptr[0], APK_BLOB_PTR_LEN(line.ptr+2, line.len-2));
	return 0;
}

struct apk_package *apk_pkg_parse_index_entry(struct apk_database *db, apk_blob_t blob)
{
	struct read_info_ctx ctx;

	ctx.pkg = apk_pkg_new();
	if (ctx.pkg == NULL)
		return NULL;

	ctx.db = db;

	apk_blob_for_each_segment(blob, "\n", parse_index_line, &ctx);

	if (ctx.pkg->name == NULL) {
		apk_pkg_free(ctx.pkg);
		apk_error("Failed to parse index entry: " BLOB_FMT,
			  BLOB_PRINTF(blob));
		ctx.pkg = NULL;
	}

	return ctx.pkg;
}

static int write_depends(struct apk_ostream *os, const char *field,
			 struct apk_dependency_array *deps)
{
	int r;

	if (deps->num == 0) return 0;
	if (apk_ostream_write(os, field, 2) != 2) return -1;
	if ((r = apk_deps_write(NULL, deps, os, APK_BLOB_PTR_LEN(" ", 1))) < 0) return r;
	if (apk_ostream_write(os, "\n", 1) != 1) return -1;
	return 0;
}

int apk_pkg_write_index_entry(struct apk_package *info,
			      struct apk_ostream *os)
{
	char buf[512];
	apk_blob_t bbuf = APK_BLOB_BUF(buf);

	apk_blob_push_blob(&bbuf, APK_BLOB_STR("C:"));
	apk_blob_push_csum(&bbuf, &info->csum);
	apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nP:"));
	apk_blob_push_blob(&bbuf, APK_BLOB_STR(info->name->name));
	apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nV:"));
	apk_blob_push_blob(&bbuf, *info->version);
	if (info->arch != NULL) {
		apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nA:"));
		apk_blob_push_blob(&bbuf, *info->arch);
	}
	apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nS:"));
	apk_blob_push_uint(&bbuf, info->size, 10);
	apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nI:"));
	apk_blob_push_uint(&bbuf, info->installed_size, 10);
	apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nT:"));
	apk_blob_push_blob(&bbuf, APK_BLOB_STR(info->description));
	apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nU:"));
	apk_blob_push_blob(&bbuf, APK_BLOB_STR(info->url));
	apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nL:"));
	apk_blob_push_blob(&bbuf, *info->license);
	if (info->origin) {
		apk_blob_push_blob(&bbuf, APK_BLOB_STR("\no:"));
		apk_blob_push_blob(&bbuf, *info->origin);
	}
	if (info->maintainer) {
		apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nm:"));
		apk_blob_push_blob(&bbuf, *info->maintainer);
	}
	if (info->build_time) {
		apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nt:"));
		apk_blob_push_uint(&bbuf, info->build_time, 10);
	}
	if (info->commit) {
		apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nc:"));
		apk_blob_push_blob(&bbuf, APK_BLOB_STR(info->commit));
	}
	if (info->provider_priority) {
		apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nk:"));
		apk_blob_push_uint(&bbuf, info->provider_priority, 10);
	}
	apk_blob_push_blob(&bbuf, APK_BLOB_STR("\n"));

	if (APK_BLOB_IS_NULL(bbuf)) {
		apk_error("Metadata for package " PKG_VER_FMT " is too long.",
			  PKG_VER_PRINTF(info));
		return -ENOBUFS;
	}

	bbuf = apk_blob_pushed(APK_BLOB_BUF(buf), bbuf);
	if (apk_ostream_write(os, bbuf.ptr, bbuf.len) != bbuf.len ||
	    write_depends(os, "D:", info->depends) ||
	    write_depends(os, "p:", info->provides) ||
	    write_depends(os, "i:", info->install_if))
		return -EIO;

	return 0;
}

int apk_pkg_version_compare(struct apk_package *a, struct apk_package *b)
{
	if (a->version == b->version)
		return APK_VERSION_EQUAL;

	return apk_version_compare_blob(*a->version, *b->version);
}

unsigned int apk_foreach_genid(void)
{
	static unsigned int foreach_genid;
	foreach_genid += (~APK_FOREACH_GENID_MASK) + 1;
	return foreach_genid;
}

void apk_pkg_foreach_matching_dependency(
		struct apk_package *pkg, struct apk_dependency_array *deps,
		unsigned int match, struct apk_package *mpkg,
		void cb(struct apk_package *pkg0, struct apk_dependency *dep0, struct apk_package *pkg, void *ctx),
		void *ctx)
{
	unsigned int genid = match & APK_FOREACH_GENID_MASK;
	unsigned int one_dep_only = genid && !(match & APK_FOREACH_DEP);
	struct apk_dependency *d;

	if (pkg && genid) {
		if (pkg->foreach_genid >= genid)
			return;
		pkg->foreach_genid = genid;
	}

	foreach_array_item(d, deps) {
		if (apk_dep_analyze(d, mpkg) & match) {
			cb(pkg, d, mpkg, ctx);
			if (one_dep_only) break;
		}
	}
}

static void foreach_reverse_dependency(
		struct apk_package *pkg,
		struct apk_name_array *rdepends,
		unsigned int match,
		void cb(struct apk_package *pkg0, struct apk_dependency *dep0, struct apk_package *pkg, void *ctx),
		void *ctx)
{
	unsigned int genid = match & APK_FOREACH_GENID_MASK;
	unsigned int marked = match & APK_FOREACH_MARKED;
	unsigned int installed = match & APK_FOREACH_INSTALLED;
	unsigned int one_dep_only = genid && !(match & APK_FOREACH_DEP);
	struct apk_name **pname0, *name0;
	struct apk_provider *p0;
	struct apk_package *pkg0;
	struct apk_dependency *d0;

	foreach_array_item(pname0, rdepends) {
		name0 = *pname0;
		foreach_array_item(p0, name0->providers) {
			pkg0 = p0->pkg;
			if (installed && pkg0->ipkg == NULL)
				continue;
			if (marked && !pkg0->marked)
				continue;
			if (genid) {
				if (pkg0->foreach_genid >= genid)
					continue;
				pkg0->foreach_genid = genid;
			}
			foreach_array_item(d0, pkg0->depends) {
				if (apk_dep_analyze(d0, pkg) & match) {
					cb(pkg0, d0, pkg, ctx);
					if (one_dep_only) break;
				}
			}
		}
	}
}

void apk_pkg_foreach_reverse_dependency(
		struct apk_package *pkg, unsigned int match,
		void cb(struct apk_package *pkg0, struct apk_dependency *dep0, struct apk_package *pkg, void *ctx),
		void *ctx)
{
	struct apk_dependency *p;

	foreach_reverse_dependency(pkg, pkg->name->rdepends, match, cb, ctx);
	foreach_array_item(p, pkg->provides)
		foreach_reverse_dependency(pkg, p->name->rdepends, match, cb, ctx);
}
