#!/bin/sh

# apk add \
#	abuild apk-tools alpine-conf busybox fakeroot syslinux xorriso
#	(for efi:) mtools dosfstools grub-efi

# FIXME: clean workdir out of unneeded sections
# FIXME: --release: cp/mv images to REPODIR/$ARCH/releases/
# FIXME: --update-latest: rewrite latest-releases.yaml with this build

set -e


scriptrealpath="$(realpath "$0")"
scriptname="${scriptrealpath##*/}"
scriptrealdir="$(dirname "$scriptrealpath")"

scriptdir="${scriptrealdir}"

# Include utilities: 'basic', 'list', 'fkrt'
. "$scriptdir/utils/utils-basic.sh"
. "$scriptdir/utils/utils-info.sh"
. "$scriptdir/utils/utils-list.sh"
. "$scriptdir/utils/utils-search.sh"
. "$scriptdir/utils/utils-fkrt.sh"
. "$scriptdir/utils/utils-plugin-loader.sh"

default_colors
info_prog_set "$scriptname"


checksum() {
	sha1sum | cut -f 1 -d ' '
}

###
### Begin code for mkimage proper
###

usage() {
	cat <<EOF

$0	[--tag RELEASE] [--outdir OUTDIR] [--workdir WORKDIR]
		[--arch ARCH] [--profile PROFILE] [--hostkeys] [--simulate]
		[--repository REPO] [--extra-repository REPO] [--yaml FILE]
$0	--help

options:
--arch			Specify which architecture images to build
			(default: $default_arch)
--hostkeys		Copy system apk signing keys to created images
--outdir		Specify directory for the created images
--profile		Specify which profiles to build
--plugins		Specify a file or directory structure root from
                        which to load additional discovered plugins.
--repository-file	File to use for list of repositories
--repository		Package repository to use for the image create
--extra-repository	Add repository to search packages from
--apk-cache-dir		Sets cache directory for apks.
--simulate		Don't execute commands
--tag			Build images for tag RELEASE
--workdir		Specify temporary working directory (cache)
--yaml

known plugins     : $all_plugins
known profiles    : $all_profiles
known image types : $all_imagetypes
known features    : $all_features

EOF
}


# load plugins
load_plugins "$scriptdir"
[ -z "$HOME" ] || load_plugins "$HOME/.mkimage"


# parse parameters
while [ $# -gt 0 ]; do
	opt="$1"
	shift
	case "$opt" in
	--repository-file) REPOFILE="$1"; shift ;;
	--repository) REPODIR="$1"; shift ;;
	--extra-repository) EXTRAREPOS="$EXTRAREPOS $1"; shift ;;
	--apk-cache-dir) APK_CACHE_DIR="$1" shift ;;
	--workdir) WORKDIR="$1"; shift ;;
	--outdir) OUTDIR="$1"; shift ;;
	--tag) RELEASE="$1"; shift ;;
	--arch) req_arch="$1"; shift ;;
	--profile) req_profiles="$1"; shift ;;
	--plugin) load_plugins "$1"; shift ;;
	--hostkeys) _hostkeys="--hostkeys";;
	--simulate) _simulate="yes";;
	--checksum) _checksum="yes";;
	--yaml) _yaml="yes";;
	--) break ;;
	-*) _show_usage="yes";;
	esac
done

# Configuration of apk, use cache directory if specified
APK="abuild-apk ${APK_CACHE_DIR:+--cache-dir $APK_CACHE_DIR}"

# Global variable declarations
all_checksums="sha256 sha512"
all_arches="aarch64 armhf x86 x86_64"
all_dirs=""
build_date="$(date +%Y%m%d)"
default_arch="$($APK --print-arch)"
_hostkeys=""
_simulate=""
_checksum=""

# Location of yaml generator -- TODO: This should move!
mkimage_yaml="$(dirname $0)"/mkimage-yaml.sh

# If we're NOT running in the script directory, default to outputting in the curent directory.
# If we ARE running in our script directory, at least put output files somewhere sane, like ./out
OUTDIR="${OUTDIR:-$PWD}"

[ "$_show_usage" = "yes" ] && usage ; exit 1

# Save ourselves from making a mess in our script's root directory.
mkdir -p "$OUTDIR"
[ "$(realpath "$OUTDIR")" = "$scriptrealdir" ] && OUTDIR="${OUTDIR}/out"
mkdir -p "$OUTDIR"

# Setup default workdir in /tmp using mktemp
if [ -z "$WORKDIR" ]; then
	WORKDIR="/tmp/$(mktemp -d -t mkimage.XXXXXX)"
	trap 'rm -rf $WORKDIR' INT
	mkdir -p "$WORKDIR"
fi


# Release configuration
RELEASE="${RELEASE:-${build_date}}"

if [ -z "$RELEASE" ]; then
	if git describe --exact-match >/dev/null 2>&1; then
		RELEASE=$(git describe --always)
		RELEASE=${RELEASE#v}
	else
		RELEASE="${build_date}"
	fi
fi

req_profiles=${req_profiles:-${all_profiles}}
req_arch=${req_arch:-${default_arch}}
[ "$req_arch" != "all" ] || req_arch="${all_arch}"
[ "$req_profiles" != "all" ] || req_profiles="${all_profiles}"


# get abuild pubkey used to sign the apkindex
# we need inject this to the initramfs or we will not be able to use the
# boot repository
if [ -z "$_hostkeys" ]; then
	_pub=${PACKAGER_PRIVKEY:+${PACKAGER_PRIVKEY}.pub}
	_abuild_pubkey="${PACKAGER_PUBKEY:-$_pub}"
fi

# create images
for ARCH in $req_arch; do
	info_prog_set "$scriptname:$ARCH"

	APKROOT="$WORKDIR/apkroot-$ARCH"
	APKREPOS="$APKROOT/etc/apk/repositories"
	if [ ! -e "$APKROOT" ]; then
		# create root for caching packages
		mkdir -p "$APKROOT/etc/apk/cache"
		cp -Pr /etc/apk/keys "$APKROOT/etc/apk/"
		$APK --arch "$ARCH" --root "$APKROOT" add --initdb

		if [ -z "$REPODIR" ] && [ -z "$REPOFILE" ]; then
			warning "no repository set"
		fi
		
		touch "$APKREPOS"

		for repo in $REPOFILE ; do
			cat "$REPOFILE" | grep -E -v "^#" >> "$APKREPOS"
		done

		[ -z "$REPODIR"] || echo "$REPODIR" >> "$APKREPOS"

		for repo in $EXTRAREPOS; do
			echo "$repo" >> "$APKREPOS"
		done
	fi
	$APK update --root "$APKROOT"

	if [ "$_yaml" = "yes" ]; then
		_yaml_out=${OUTDIR:-.}/latest-releases.yaml
		echo "---" > "$_yaml_out"
	fi
	for PROFILE in $req_profiles; do
		info_prog_set "$scriptname:$ARCH:$PROFILE"
		msg "Beginning build for profile $PROFILE."
		(build_profile) || exit 1
	done

	info_prog_set ""
done
echo "Images generated in $OUTDIR"
