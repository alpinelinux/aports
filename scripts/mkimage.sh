#!/bin/sh

# apk add \
#	abuild apk-tools alpine-conf busybox fakeroot syslinux xorriso cmd:mksquashfs
#	(for efi:) mtools
#	(for s390x:) s390-tools
#	(for ppc64le:) grub

# FIXME: clean workdir out of unneeded sections
# FIXME: --release: cp/mv images to REPODIR/$ARCH/releases/
# FIXME: --update-latest: rewrite latest-releases.yaml with this build

set -e

# get abuild configurables
. /usr/share/abuild/functions.sh

scriptdir="$(dirname "$0")"
git=$(command -v git) || git=true

# deduce aports directory
[ -n "$APORTS" ] || APORTS=$(realpath "$scriptdir/../")
[ -e "$APORTS/main/build-base" ] || die "Unable to deduce aports base checkout"

# echo '-dirty' if git is not clean
git_dirty() {
	[ $($git status -s -- "$scriptdir" | wc -l) -ne 0 ] && echo "-dirty"
}

# echo last commit hash id
git_last_commit() {
	$git log --format=oneline -n 1 -- "$scriptdir" | awk '{print $1}'
}

# date of last commit
git_last_commit_epoch() {
	$git log -1 --format=%cd --date=unix "$1" -- "$scriptdir"
}

set_source_date() {
	# dont error out if we're not in git
	if ! $git rev-parse --show-toplevel >/dev/null 2>&1; then
		git=true
	fi
	# set time stamp for reproducible builds
	if [ -z "$ABUILD_LAST_COMMIT" ]; then
		export ABUILD_LAST_COMMIT="$(git_last_commit)$(git_dirty)"
	fi
	if [ -z "$SOURCE_DATE_EPOCH" ] && [ "${ABUILD_LAST_COMMIT%-dirty}" = "$ABUILD_LAST_COMMIT" ]; then
		SOURCE_DATE_EPOCH=$(git_last_commit_epoch "$ABUILD_LAST_COMMIT")
	fi
	if [ -z "$SOURCE_DATE_EPOCH" ]; then
		SOURCE_DATE_EPOCH=$(date -u "+%s")
	fi
	export SOURCE_DATE_EPOCH
}

set_source_date

# 
all_sections=""
all_profiles=""
all_checksums="sha256 sha512"
all_dirs=""
build_date="$(date -u +%y%m%d -d "@$SOURCE_DATE_EPOCH")"
default_arch="$(apk --print-arch)"
_hostkeys=""
_simulate=""
_checksum=""

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
		[--repository REPO [--repository REPO]]
		[--repositories-file REPO_FILE] [--yaml FILE]
$0	--help

options:
--arch			Specify which architecture images to build
			(default: $default_arch)
--hostkeys		Copy system apk signing keys to created images
--outdir		Specify directory for the created images
--profile		Specify which profiles to build
--repositories-file	List of repositories to use for the image create
--repository		Package repository to use for the image create
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
		create_image_${output_format} || { _fail="yes"; return 1; }

		if [ "$_checksum" = "yes" ]; then
			for _c in $all_checksums; do
				echo "$(${_c}sum "$output_file" | cut -d' '  -f1)  ${output_filename}" > "${output_file}.${_c}"
			done
		fi
	fi
	if [ -n "$_yaml_out" ]; then
		$mkimage_yaml --release $RELEASE \
			--title "$title" \
			--desc "$desc" \
			"$output_file" >> "$_yaml_out"
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
	--repositories-file) REPOS_FILE="$1"; shift ;;
	--repository)
		if [ -z "$REPOS" ]; then
			REPOS="$1"
		else
			REPOS=$(printf '%s\n%s' "$REPOS" "$1");
		fi
		shift ;;
	--extra-repository)
		warning "--extra-repository is deprecated. Use multiple --repository"
		EXTRAREPOS="$EXTRAREPOS $1"
		shift ;;
	--workdir) WORKDIR="$1"; shift ;;
	--outdir) OUTDIR="$1"; shift ;;
	--tag) RELEASE="$1"; shift ;;
	--arch) req_arch="$1"; shift ;;
	--profile) req_profiles="$1"; shift ;;
	--hostkeys) _hostkeys="--hostkeys";;
	--simulate) _simulate="yes";;
	--checksum) _checksum="yes";;
	--yaml) _yaml="yes";;
	--help) usage; exit 0;;
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

if [ -z "$REPOS" ]; then
  echo "Must provide --repository"
  exit 2
fi

# setup defaults
if [ -z "$WORKDIR" ]; then
	WORKDIR="$(mktemp -d -t mkimage.XXXXXX)"
	trap 'rm -rf $WORKDIR' INT EXIT
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
		mkdir -p "$APKROOT/etc/apk/cache" "$APKROOT"/etc/apk/keys
		[ -d /usr/share/apk/keys/"$ARCH" ] &&
			cp /usr/share/apk/keys/"$ARCH"/* "$APKROOT"/etc/apk/keys
		if [ -n "$_hostkeys" ]; then
			cp /etc/apk/keys/* "$APKROOT"/etc/apk/keys
		else
			cp "$_abuild_pubkey" "$APKROOT"/etc/apk/keys
		fi
		apk --arch "$ARCH" --root "$APKROOT" add --initdb

		_repositories="$APKROOT/etc/apk/repositories"
		if [ -n "$REPOS_FILE" ]; then
			cat "$REPOS_FILE" > "$_repositories"
		fi
		echo "$REPOS" >> "$_repositories"
		for repo in $EXTRAREPOS; do
			echo "$repo" >> "$_repositories"
		done
	fi
	apk update --root "$APKROOT"

	if [ "$_yaml" = "yes" ]; then
		_yaml_out=${OUTDIR:-.}/latest-releases.yaml
		echo "---" > "$_yaml_out"
	fi
	for PROFILE in $req_profiles; do
		(set -eo pipefail; build_profile) || exit 1
	done
done
echo "Images generated in $OUTDIR"
