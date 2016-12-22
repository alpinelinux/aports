#!/bin/sh

# apk add \
#	abuild apk-tools alpine-conf busybox fakeroot syslinux xorriso
#	(for efi:) mtools dosfstools grub-efi

# FIXME: clean workdir out of unneeded sections
# FIXME: --release: cp/mv images to REPODIR/$ARCH/releases/
# FIXME: --update-latest: rewrite latest-releases.yaml with this build

set -e

# get abuild configurables
[ -e /usr/share/abuild/functions.sh ] || (echo "abuild not found" ; exit 1)
. /usr/share/abuild/functions.sh

# deduce aports directory
[ -n "$APORTS" ] || APORTS=$(realpath $(dirname $0)/../)
[ -e "$APORTS/main/build-base" ] || die "Unable to deduce aports base checkout"

# 
all_sections=""
all_profiles=""
all_checksums="sha256 sha512"
all_arches="aarch64 armhf x86 x86_64"
all_dirs=""
build_date="$(date +%y%m%d)"
default_arch="$(apk --print-arch)"
_hostkeys=""
_simulate=""
_checksum=""

scriptdir="$(dirname $0)"
OUTDIR="$PWD"
RELEASE="${build_date}"


msg() {
	if [ -n "$quiet" ]; then return 0; fi
	local prompt="$GREEN>>>${NORMAL}"
	local name="${BLUE}mkimage${ARCH+-$ARCH}${NORMAL}"
	printf "${prompt} ${name}: %s\n" "$1" >&2
}

list_has() {
	local needle="$1"
	local i
	shift
	for i in $@; do
		[ "$needle" != "$i" ] || return 0
	done
	return 1
}

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
--repository		Package repository to use for the image create
--extra-repository	Add repository to search packages from
--simulate		Don't execute commands
--tag			Build images for tag RELEASE
--workdir		Specify temporary working directory (cache)
--yaml

known profiles: $(echo $all_profiles | sort -u)

EOF
}

# helpers
load_plugins() {
	local f
	[ -e "$1" ] || return 0
	for f in "$1"/mkimg.*.sh; do
		[ -e "$f" ] || return 0
		break
	done
	all_profiles="$all_profiles $(sed -n -e 's/^profile_\(.*\)() {$/\1/p' $1/mkimg.*.sh)"
	all_sections="$all_sections $(sed -n -e 's/^section_\(.*\)() {$/\1/p' $1/mkimg.*.sh)"
	for f in "$1"/mkimg.*.sh; do
		. $f
	done
}

checksum() {
	sha1sum | cut -f 1 -d ' '
}

build_section() {
	local section="$1"
	local args="$@"
	local _dir="${args//[^a-zA-Z0-9]/_}"
	shift
	local args="$@"

	if [ -z "$_dir" ]; then
		_fail="yes"
		return 1
	fi

	if [ ! -e "$WORKDIR/${_dir}" ]; then
		DESTDIR="$WORKDIR/${_dir}.work"
		msg "--> $section $args"
		if [ -z "$_simulate" ]; then
			rm -rf "$DESTDIR"
			mkdir -p "$DESTDIR"
			if build_${section} "$@"; then
				mv "$DESTDIR" "$WORKDIR/${_dir}"
				_dirty="yes"
			else
				rm -rf "$DESTDIR"
				_fail="yes"
				return 1
			fi
		fi
	fi
	unset DESTDIR
	all_dirs="$all_dirs $_dir"
	_my_sections="$_my_sections $_dir"
}

build_profile() {
	local _id _dir _spec
	_my_sections=""
	_dirty="no"
	_fail="no"

	profile_$PROFILE
	list_has $ARCH $arch || return 0

	msg "Building $PROFILE"

	# Collect list of needed sections, and make sure they are built
	for SECTION in $all_sections; do
		section_$SECTION || return 1
	done
	[ "$_fail" = "no" ] || return 1

	# Defaults
	[ -n "$image_name" ] || image_name="alpine-${PROFILE}"
	[ -n "$output_filename" ] || output_filename="${image_name}-${RELEASE}-${ARCH}.${image_ext}"
	local output_file="${OUTDIR:-.}/$output_filename"

	# Construct final image
	local _imgid=$(echo -n $_my_sections | sort | checksum)
	DESTDIR=$WORKDIR/image-$_imgid-$ARCH-$PROFILE
	if [ "$_dirty" = "yes" -o ! -e "$DESTDIR" ]; then
		msg "Creating $output_filename"
		if [ -z "$_simulate" ]; then
			# Merge sections
			rm -rf "$DESTDIR"
			mkdir -p "$DESTDIR"
			for _dir in $_my_sections; do
				for _fn in $WORKDIR/$_dir/*; do
					[ ! -e "$_fn" ] || cp -Lrs $_fn $DESTDIR/
				done
			done
			echo "${image_name}-${RELEASE} ${build_date}" > "$DESTDIR"/.alpine-release
		fi
	fi

	if [ "$_dirty" = "yes" -o ! -e "$output_file" ]; then
		# Create image
		[ -n "$output_format" ] || output_format="${image_ext//[:\.]/}"
		create_image_${output_format} || { _fail="yes"; false; }

		if [ "$_checksum" = "yes" ]; then
			for _c in $all_checksums; do
				echo "$(${_c}sum "$output_file" | cut -d' '  -f1)  ${output_filename}" > "${output_file}.${_c}"
			done
		fi

		if [ -n "$_yaml_out" ]; then
			$mkimage_yaml --release $RELEASE \
				"$output_file" >> "$_yaml_out"
		fi
	fi
}

# load plugins
load_plugins "$scriptdir"
[ -z "$HOME" ] || load_plugins "$HOME/.mkimage"

mkimage_yaml="$(dirname $0)"/mkimage-yaml.sh

# parse parameters
while [ $# -gt 0 ]; do
	opt="$1"
	shift
	case "$opt" in
	--repository) REPODIR="$1"; shift ;;
	--extra-repository) EXTRAREPOS="$EXTRAREPOS $1"; shift ;;
	--workdir) WORKDIR="$1"; shift ;;
	--outdir) OUTDIR="$1"; shift ;;
	--tag) RELEASE="$1"; shift ;;
	--arch) req_arch="$1"; shift ;;
	--profile) req_profiles="$1"; shift ;;
	--hostkeys) _hostkeys="--hostkeys";;
	--simulate) _simulate="yes";;
	--checksum) _checksum="yes";;
	--yaml) _yaml="yes";;
	--) break ;;
	-*) usage; exit 1;;
	esac
done

if [ -z "$RELEASE" ]; then
	if git describe --exact-match >/dev/null 2>&1; then
		RELEASE=$(git describe --always)
		RELEASE=${RELEASE#v}
	else
		RELEASE="${build_date}"
	fi
fi

# setup defaults
if [ -z "$WORKDIR" ]; then
	WORKDIR="$(mktemp -d -t mkimage.XXXXXX)"
	trap 'rm -rf $WORKDIR' INT
	mkdir -p "$WORKDIR"
fi
req_profiles=${req_profiles:-${all_profiles}}
req_arch=${req_arch:-${default_arch}}
[ "$req_arch" != "all" ] || req_arch="${all_arch}"
[ "$req_profiles" != "all" ] || req_profiles="${all_profiles}"

mkdir -p "$OUTDIR"

# get abuild pubkey used to sign the apkindex
# we need inject this to the initramfs or we will not be able to use the
# boot repository
if [ -z "$_hostkeys" ]; then
	_pub=${PACKAGER_PRIVKEY:+${PACKAGER_PRIVKEY}.pub}
	_abuild_pubkey="${PACKAGER_PUBKEY:-$_pub}"
fi

# create images
for ARCH in $req_arch; do
	APKROOT="$WORKDIR/apkroot-$ARCH"
	if [ ! -e "$APKROOT" ]; then
		# create root for caching packages
		mkdir -p "$APKROOT/etc/apk/cache"
		cp -Pr /etc/apk/keys "$APKROOT/etc/apk/"
		abuild-apk --arch "$ARCH" --root "$APKROOT" add --initdb

		if [ -z "$REPODIR" ]; then
			warning "no repository set"
		fi
		echo "$REPODIR" > "$APKROOT/etc/apk/repositories"
		for repo in $EXTRAREPOS; do
			echo "$repo" >> "$APKROOT/etc/apk/repositories"
		done
	fi
	abuild-apk update --root "$APKROOT"

	if [ "$_yaml" = "yes" ]; then
		_yaml_out=${OUTDIR:-.}/latest-releases.yaml
		echo "---" > "$_yaml_out"
	fi
	for PROFILE in $req_profiles; do
		(build_profile) || exit 1
	done
done
echo "Images generated in $OUTDIR"
