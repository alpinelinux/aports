/*
 * ID Mapping Plugin interface for cifs-utils
 * Copyright (C) 2012 Jeff Layton (jlayton@samba.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>

#include <sys/types.h>

#ifndef _CIFSIDMAP_H
#define _CIFSIDMAP_H

#define NUM_AUTHS (6)			/* number of authority fields */
#define SID_MAX_SUB_AUTHORITIES (15)	/* max number of sub authority fields */

/*
 * Binary representation of a SID as presented to/from the kernel. Note that
 * the sub_auth field is always stored in little-endian here.
 */
struct cifs_sid {
	uint8_t revision; /* revision level */
	uint8_t num_subauth;
	uint8_t authority[NUM_AUTHS];
	uint32_t sub_auth[SID_MAX_SUB_AUTHORITIES];
} __attribute__((packed));


/*
 * The type of the ID stored within cifs_uxid. UNKNOWN generally means that
 * the mapping failed for some reason. BOTH means that the ID is usable as
 * either a UID or a GID -- IOW, the UID and GID namespaces are unity-mapped.
 */
#define	CIFS_UXID_TYPE_UNKNOWN	(0)	/* mapping type is unknown */
#define	CIFS_UXID_TYPE_UID	(1)	/* mapping is a UID */
#define	CIFS_UXID_TYPE_GID	(2)	/* mapping is a GID */
#define	CIFS_UXID_TYPE_BOTH	(3)	/* usable as UID or GID */

/* This struct represents a uid or gid and its type */
struct cifs_uxid {
	union {
		uid_t uid;
		gid_t gid;
	} id;
	unsigned char type;
} __attribute__((packed));

/*
 * Plugins should implement the following functions:
 */

/**
 * cifs_idmap_init_plugin - Initialize the plugin interface
 * @handle - return pointer for an opaque handle
 * @errmsg - pointer to error message pointer
 *
 * This function should do whatever is required to establish a context
 * for later ID mapping operations. The "handle" is an opaque context
 * cookie that will be passed in on subsequent ID mapping operations.
 * The errmsg is used to pass back an error string both during the init
 * and in subsequent idmapping functions. On any error, the plugin
 * should point *errmsg at a string describing that error. Returns 0
 * on success and non-zero on error.
 */
extern int cifs_idmap_init_plugin(void **handle, const char **errmsg);

/**
 * cifs_idmap_exit_plugin - Destroy an idmapping context
 * @handle - context handle that should be destroyed
 *
 * When programs are finished with the idmapping plugin, they'll call
 * this function to destroy any context that was created during the
 * init_plugin. The handle passed back in was the one given by the init
 * routine.
 */
extern void cifs_idmap_exit_plugin(void *handle);

/**
 * cifs_idmap_sid_to_str - convert cifs_sid to a string
 * @handle - context handle
 * @sid    - pointer to a cifs_sid
 * @name   - return pointer for the name
 *
 * This function should convert the given cifs_sid to a string
 * representation or mapped name in a heap-allocated buffer. The caller
 * of this function is expected to free "name" on success. Returns 0 on
 * success and non-zero on error. On error, the errmsg pointer passed
 * in to the init_plugin function should point to an error string. The
 * caller will not free the error string.
 */
extern int cifs_idmap_sid_to_str(void *handle, const struct cifs_sid *sid,
					char **name);

/**
 * cifs_idmap_str_to_sid - convert string to struct cifs_sid
 * @handle - context handle
 * @name   - pointer to name string to be converted
 * @sid    - pointer to struct cifs_sid where result should go
 *
 * This function converts a name string or string representation of
 * a SID to a struct cifs_sid. The cifs_sid should already be
 * allocated. Returns 0 on success and non-zero on error. On error, the
 * plugin should reset the errmsg pointer passed to the init_plugin
 * function to an error string. The caller will not free the error string.
 */
extern int cifs_idmap_str_to_sid(void *handle, const char *name,
				struct cifs_sid *sid);

/**
 * cifs_idmap_sids_to_ids - convert struct cifs_sids to struct cifs_uxids
 * @handle - context handle
 * @sid    - pointer to array of struct cifs_sids to be converted
 * @num    - number of sids to be converted
 * @cuxid  - pointer to preallocated array of struct cifs_uxids for return
 *
 * This function should map an array of struct cifs_sids to an array of
 * struct cifs_uxids.
 *
 * Returns 0 if at least one conversion was successful and non-zero on error.
 * Any that were not successfully converted will have a cuxid->type of
 * CIFS_UXID_TYPE_UNKNOWN.
 *
 * On any error, the plugin should reset the errmsg pointer passed to the
 * init_plugin function to an error string. The caller will not free the error
 * string.
 */
extern int cifs_idmap_sids_to_ids(void *handle, const struct cifs_sid *sid,
				const size_t num, struct cifs_uxid *cuxid);

/**
 * cifs_idmap_ids_to_sids - convert uid to struct cifs_sid
 * @handle - context handle
 * @cuxid  - pointer to array of struct cifs_uxid to be converted to SIDs
 * @num    - number of cifs_uxids to be converted to SIDs
 * @sid    - pointer to preallocated array of struct cifs_sid where results
 * 	     should be stored
 *
 * This function should map an array of cifs_uxids an array of struct cifs_sids.
 * Returns 0 if at least one conversion was successful and non-zero on error.
 * Any sids that were not successfully converted should have their revision
 * number set to 0.
 *
 * On any error, the plugin should reset the errmsg pointer passed to the
 * init_plugin function to an error string. The caller will not free the error
 * string.
 */
extern int cifs_idmap_ids_to_sids(void *handle, const struct cifs_uxid *cuxid,
				const size_t num, struct cifs_sid *sid);
#endif /* _CIFSIDMAP_H */
