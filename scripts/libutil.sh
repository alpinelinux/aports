#!/bin/sh

# libutil.sh - Utility functions
#
# Copyright(c) 2005 Natanael Copa
#
# Distributed under GPL-2
#

VERSION=0.13.1

# echo to stderr
eecho() {
	echo $* >&2
}

# echo to stderr and die
die() {
	echo -n "$PROGRAM: " >&2
	eecho $*
	exit 1
}

die_unless_force() {
	echo "$PROGRAM: $*" >&2
	[ -z "$FORCE" ] && exit 1
}

# remove double / and ./ in pathnames
beautify_path() {
	echo "$1" | sed 's:/^[^\.]\./::g; s:/\{2,\}:/:g; s:/\./:/:g'
}

# check if parameter is an uri or not
is_uri() {
	echo "$1" | grep "^[a-z][a-z0-9+]*:/" >/dev/null
}

# check if parameter is an apk package (contains a / or .apk at the end)
is_apk() {
	#echo "$1" | grep '/' >/dev/null && return 0
	[ -z "${1%%*/*}" ] && return 0
	
	#echo "$1" | grep ".apk$" >/dev/null
	[ -z "${1%%*.apk}" ]
}

# check if path start with a '/'
is_absolute_path() {
	test -z "${1##/*}"
}

# if path dont start with '/' then append $PWD
get_absolute_path() {
	if is_absolute_path "$1" ; then
		echo "$1"
	else
		beautify_path "$PWD/$1"
	fi
}

# check if parameter has version number (i.e. if it is an pkgv or pkg)
has_version() {
	echo "$1" | grep -- '-[0-9].*' >/dev/null
}

# check if parameter has some kind of wildcard
has_wildcard() {
	echo "$1" | grep "[\*\?\[]" >/dev/null
}

# get the scheme for an uri (echo everything before the first ':')
get_uri_scheme() {
	echo "$1" | cut -d : -f 1
}

# remove version number from package name
rm_ver() {
	echo "$1" | sed 's/\(.*\)-[0-9].*/\1/'
}

# get version number from package name or file
get_ver() {
	basename "$1" .apk | sed 's/.*-\([0-9].*\)/\1/'
}

# initialize a temp directory
# $1 contains the variable name for the directory
# the directory will automatically be deleted upon exit
init_tmpdir() {
	local omask=`umask`
	local __tmpd="$APK_TMPDIR/$PROGRAM-${$}-`date +%s`"
	umask 077 || die "umask"
	mkdir "$__tmpd" || exit 1
	trap "rm -fr \"$__tmpd\"; exit" 0
	umask $omask
	eval "$1=\"$__tmpd\""
}

# remove files and empty dirs in specified list.
# also remove APK_LBUFILES from default.tdb
# environment:
#  ROOT:	all files are relative this path
#  VERBOSE: 	echo filenames to stdout
#  DRYRUN:  	don't delete anything, just simulate
my_rm() {
	rm "$1" 2>/dev/null || busybox rm "$1"
}

list_uninstall() {
	local f p 
	local root=${ROOT:-"/"}
	sort -r "$1" | while read f ; do
		p="`beautify_path \"$root/$f\"`"
		if [ "$DRYRUN" ] ; then
			[ "$VERBOSE" ] && echo "$p"
		else
			if [ -d "$p" ] ; then
				# try to remove dir, but ignore errors. It might
				if rmdir "$p" 2>/dev/null ; then
					[ "$VERBOSE" ] && echo "$p"
					[ "$2" ] && echo "$f" >> "$2"
				fi
			else
				my_rm "$p" && [ "$VERBOSE" ] && echo "$p"
				[ "$2" ] && echo "$f" >> "$2"
			fi
		fi
	done
	return 0
}

# list all lines that occur in first list but not second
# the files cannot contain duplicate lines.
list_subtract() {
	( 
		# first we find all uniq lines
		cat "$1" "$2" | sort | uniq -u 
		
		# then we combine uniq lines with first file ...
		cat "$1"

		# ...and find all duplicates. Those only exist in first file
	) | sort | uniq -d
}

# insert an element first in APK_PATH if its not already there
insert_apk_path() {
	if [ "$1" != "`echo "$APK_PATH" | cut -d\; -f1`" ] ; then
		[ "$APK_PATH" ] && APK_PATH=";$APK_PATH"
		APK_PATH="$1$APK_PATH"
	fi
}

lbu_filter() {
	# Ok... I give up. shell is too slow. lets do it in awk.
	awk 'BEGIN {
		APK_LBUDIRS="'"$APK_LBUDIRS"'";
		numdirs = split(APK_LBUDIRS, lbudir, ":");
		#precalc lengths to save a few cpu cycles in loop
		for (i = 1; i <= numdirs; i++)
			len[i] = length(lbudir[i]);
	}

	# main loop
	{
		for (i = 1; i <= numdirs; i++) {
			if (index($0, lbudir[i]) == 1 && (len[i] == length() || substr($0, len[i] + 1, 1) == "/")) {
				print $0;
			}
		}	
	}'
}

is_lbu_file() {
	# just run test
	[ "$(echo "$1" | lbu_filter)" ]
}

# assign a value to a global var, either from environment or
# from configuraion file
# usage: get_var VARIBALE_NAME DEFAULT_VALUE
get_var() {
	local var
	# first we check if the envvar is set
	eval "var=\$$1"
	if [ "$var" ] ; then
		echo "$var"
	elif [ -f ${APKTOOLS_CONF:="$ROOT/etc/apk.conf"} ] ; then
		# then we check the conf file
		var=`awk -F = '/^'$1'=/ { print $2 }' "$APKTOOLS_CONF"`
		if [ "$var" ] ; then
			echo "$var"
		else
			# else we use the default
			echo "$2"
		fi
	else
		# no conf file found use default
		echo "$2"
	fi
}

##########################################################
# last_pkgf
# find the latest package in a list, return 1 if not found
last_pkgf() {
	local pkgf last  status
	while read pkgf ; do
		apk_version -q -t "$pkgf" "$last"
		[ $? -eq 2 ] && last="$pkgf"
	done
	[ -z "$last" ] && return 1
	echo "$last"
}

###########################################################
# dump global variables
dump_env() {
	echo "ROOT=$ROOT"
	echo "APKTOOLS_CONF=$APKTOOLS_CONF"
	echo "APK_PATH=$APK_PATH"
	echo "APK_DBDIR=$APK_DBDIR"
	echo "APK_TMPDIR=$APK_TMPDIR"
	echo "APK_FETCH=$APK_FETCH"
	echo "APK_DATA=$APK_DATA"
	echo "APK_DATALEVEL=$APK_DATALEVEL"
	echo "APK_LIBS=$APK_LIBS"
	echo "PACKAGES=$PACKAGES"

	echo "APKDB=$APKDB"
	echo "APK_NOCOMPRESS=$APK_NOCOMPRESS"
	echo "REP_DIR=$REP_DIR"
	echo "REP_SCHEME=$REP_SCHEME"
	echo "CACHED_INDEX=$CACHED_INDEX"
}

#############################################################################
# init_globals sets up the global variables

APK_PREFIX_IN_PKG="`get_var APK_PREFIX_IN_PKG ''`"

ROOT="`get_var ROOT /`"
echo "$ROOT" | grep -v "^/" > /dev/null && ROOT="$PWD/$ROOT"

APKTOOLS_CONF="`get_var APKTOOLS_CONF \"$(beautify_path /etc/apk/apk.conf)\"`"
APK_PATH=`get_var APK_PATH ""`
APK_DBDIR="`get_var APK_DBDIR \"$(beautify_path \"$ROOT/var/db/apk\")\"`"
APK_DBDIR_IN_PKG="`get_var APK_DBDIR_IN_PKG ${APK_PREFIX_IN_PKG}var/db/apk`"
APK_TMPDIR="`get_var \"APK_TMPDIR\" /tmp`"
APK_ADD_TMP="`get_var \"APK_ADD_TMP\" \"$ROOT/usr/tmp\"`"
APK_DATA="`get_var APK_DATA \"$(beautify_path \"$ROOT/var/lib/apk\")\"`"
APK_KEEPCACHE="`get_var APK_KEEPCACHE no`"
APK_LIBS="`get_var APK_LIBS /lib/apk`"
PACKAGES="`get_var PACKAGES \"$(beautify_path \"$ROOT/var/cache/packages\")\"`"

APKDB="`beautify_path \"$APK_DBDIR\"`"
APK_NOCOMPRESS=`get_var APK_NOCOMPRESS ""`

INDEX="INDEX.md5.gz"
CACHED_INDEX="$APK_DATA/$INDEX"

APK_SUM=`get_var APK_SUM md5`
APK_MKSUM=`get_var APK_MKSUM "${APK_SUM}sum"`
APK_CHKSUM=`get_var APK_CHKSUM "${APK_SUM}sum -c"`

APK_DEFAULT_TDB=`get_var APK_DEFAULT_TDB "$APK_DATA/default.tdb"`
SFIC=`which sfic 2>/dev/null`
APK_GZSIGN_CERT=`get_var APK_GZSIGN_KEY /etc/apk/apk.crt`

# confdirs are a : spearate list of dirs relative $ROOT that are to be
# considered for local backups.
# for example: APK_LBUDIRS="etc:usr/local/etc"
APK_LBUDIRS=`get_var APK_LBUDIRS 'etc'`

