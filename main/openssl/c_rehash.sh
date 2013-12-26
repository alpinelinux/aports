#!/bin/sh
#
# Ben Secrest <blsecres@gmail.com>
#
# sh c_rehash script, scan all files in a directory
# and add symbolic links to their hash values.
#
# based on the c_rehash perl script distributed with openssl
#
# LICENSE: See OpenSSL license
# ^^acceptable?^^
#

# default certificate location
DIR=/etc/openssl

#
# use openssl to fingerprint a file
#    arguments:
#	1. the filename to fingerprint
#	2. the method to use (x509, crl)
#    returns:
#	none
#    assumptions:
#	user will capture output from last stage of pipeline
#
fingerprint()
{
    ${SSL_CMD} ${2} -fingerprint -noout -in ${1} | sed -e 's/^.*=//' -e 's/://g'
}


#
# link_hash - create links to certificate files
#    arguments:
#       1. the filename to create a link for
#	2. the type of certificate being linked (x509, crl)
#    returns:
#	0 on success, 1 otherwise
#
link_hash()
{
    local HASH=$( ${SSL_CMD} ${2} -hash -noout -in ${1} )
    local SUFFIX=0
    local LINKFILE=''
    local TAG=''

    if [ ${2} = "crl" ]
    then
    	TAG='r'
    fi

    LINKFILE=${HASH}.${TAG}${SUFFIX}

    if [ -f ${LINKFILE} ]
    then
	local FINGERPRINT=$( fingerprint ${1} ${2} )

	while [ -f ${LINKFILE} ]
	do
	if [ ${FINGERPRINT} = $( fingerprint ${LINKFILE} ${2} ) ]
	    then
	        echo "WARNING: Skipping duplicate file ${1}" >&2
	        return 1
	    fi

	    SUFFIX=$(( ${SUFFIX} + 1 ))
	    LINKFILE=${HASH}.${TAG}${SUFFIX}
	done
    fi

    echo "${1} => ${LINKFILE}"

    # assume any system with a POSIX shell will either support symlinks or
    # do something to handle this gracefully
    ln -s ${1} ${LINKFILE}

    return 0
}


# hash_dir create hash links in a given directory
hash_dir()
{
    echo "Doing ${1}"

    cd ${1}

    ls -1 * 2>/dev/null | grep -E '^[[:xdigit:]]{8}\.r?[[:digit:]]+$' | while read FILE
    do
        [ -h "${FILE}" ] && rm "${FILE}"
    done

    ls -1 *.pem 2>/dev/null | while read FILE
    do
	local TYPE_STR=

	if grep -q '^-----BEGIN X509 CRL-----' ${FILE}; then
	    TYPE_STR="crl"
	elif grep -q -E '^-----BEGIN (X509 |TRUSTED )?CERTIFICATE-----' ${FILE}; then 
	    TYPE_STR="x509"
	else
	    echo "WARNING: ${FILE} does not contain a certificate or CRL: skipping" >&2
	    continue
	fi

	link_hash ${FILE} ${TYPE_STR}
    done
}


# choose the name of an ssl application
if [ -n "${OPENSSL}" ]
then
    SSL_CMD=$(which ${OPENSSL} 2>/dev/null)
else
    SSL_CMD=/usr/bin/openssl
    OPENSSL=${SSL_CMD}
    export OPENSSL
fi

# fix paths
PATH=${PATH}:${DIR}/bin
export PATH

# confirm existance/executability of ssl command
if ! [ -x ${SSL_CMD} ]
then
    echo "${0}: rehashing skipped ('openssl' program not available)" >&2
    exit 0
fi

# determine which directories to process
old_IFS=$IFS
if [ ${#} -gt 0 ]
then
    IFS=':'
    DIRLIST=${*}
elif [ -n "${SSL_CERT_DIR}" ]
then
    DIRLIST=$SSL_CERT_DIR
else
    DIRLIST=${DIR}/certs
fi

IFS=':'

# process directories
for CERT_DIR in ${DIRLIST}
do
    if [ -d ${CERT_DIR} -a -w ${CERT_DIR} ]
    then
        IFS=$old_IFS
        hash_dir ${CERT_DIR}
        IFS=':'
    fi
done
