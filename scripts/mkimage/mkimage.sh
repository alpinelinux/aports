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
. "$scriptdir/utils/utils-fkrt.sh"

default_colors
info_prog_set "$scriptname"



# List containing all known plugin types initially.
# Include 'sections' plugin here directly, the rest will be discovered.
var_list_alias all_plugins

set_all_plugins "sections"
var_list_alias all_sections

##
## General purpose plugins loader.
##
## Requires: 'source utils/utils-list.sh' 'var_list_alias all_plugins' 'set_all_plugins'
##
## Usage: 'load_plugins "<filename>"' to load all plugin types and plugins from a file
##        'load_plugins "<directory>"' to load all plugins found in directory hierarchy.


# Main load_plugins function which does all the work of discovering, sourcing, and loading plugin types and plugins.
load_plugins() {

	plugins_regex="${plugins_regex:-plugin\|section}"

	local target="$1"
	local f l p q func
	local _found=""
	local new_plugins=""
	local mypath mydir myfile

	( [ -e "$target" ] && [ -r "$target" ] ) || return 0
	target="$(realpath "$target")"

	info_func_set "load_plugins"
	if [ -f "$target" ] ; then
		msg "Discovering plugins from file '$target':"
	else
		msg "Discovering plugins under directory '$target':"
	fi
	
	# Find all plugin-*.sh scripts, source them, and add plugin types they define to all_plugins list.
	while IFS=$'\n' read -r f ; do
		[ -f "$f" ] && [ -r "$f" ] || continue
		f="$(realpath "$f")"
		_found=
		# Variables to be available when sourcing files
		mypath="${f}"
		mydir="${mypath%/*}"
		myfile="${mypath##*/}"

		# Find all lines containing plugin_* function definitions.
		IFS=$'\n'
		for l in $(grep -E -e '^plugin_[_[:alnum:]+[[:space:]]*\(\)[[:space:]]*\{' "$f" ) ; do
			unset IFS
			l="${l#plugin_}"
			l="${l%\(\)*}"
			if all_plugins_has_not "$l" ; then 
				# Add this plugin type to all_plugins list, regex for finding plugins, and mark for sourcing.
				add_all_plugins "$l"
				var_list_alias "all_$l"
				plugins_regex="$plugins_regex\|${l%s}"
				_found="true"
				var_list_add new_plugins "$l"
			fi
		done
		
		# Source all files in which valid plugin definitions were found.
		[ "$_found" ] && . "$f"

	done<<-EOF
		$( [ -f "$target" ] && echo "$target"; [ -d "$target" ] && find "$target" -type f -iname 'plugin-*\.sh' -exec printf '%s\n' {} \; )
	EOF
	unset IFS

	msg2 "$(printf "%s " $(get_all_plugins))"

	# Run all new plugin_* scripts found before loading plugins themselves.
	for p in $new_plugins ; do
		plugin_${p}
	done

	# Load all plugins of type defined in all_plugins list using regex built above.
	while IFS=$'\n' read -r f ; do
		[ -f "$f" ] && [ -r "$f" ] || continue
		f="$(realpath "$f")"
		_found=
		# Variables to be available when sourcing files
		mypath="${f}"
		mydir="${mypath%/*}"
		myfile="${mypath##*/}"

		# Find all lines containing any function definitions.
		IFS=$'\n'
		for func in $(grep -E -e '^[[:alnum:]]+[_[:alnum:]]+[[:space:]]*\(\)' "$f") ; do
			unset IFS
			func="${func%()*}"
			func="${func%% }"

			# Determine if the function definition has the stem of any known plugins.
			for p in $(get_all_plugins) ; do
				p="${p}"
				q="${func#${p%s}_}"

				# Won't match iff this plugin type is stem of funciton
				if [ "$q" != "$func" ] ; then
					# Add this plugin to all_<plugin>s list and mark file for sourcing.
					var_list_add "all_$p" "$q"
					_found="true"
				fi
			done
		done

		# Source all files which contain function definitions for known plugin types.
		[ "$_found" ] && . "$f"

	done<<-EOF
		$([ -f "$target" ] && echo "$target" ; [ -d "$target" ] && find "$target" -type f -regex "$target.*/\($plugins_regex\)-.*\.sh" -exec printf '%s\n' {} \; )
	EOF
	unset IFS

	info_func_set ""
}

checksum() {
	sha1sum | cut -f 1 -d ' '
}

# Empty plugin to keep load_plugins happy.
plugin_sections() {
	return 0
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

		info_func_set "build_$section"
		msg "Building section '$section' with args '$args':"
		if [ -z "$_simulate" ]; then
			rm -rf "$DESTDIR"
			mkdir -p "$DESTDIR"
			if build_${section} "$@"; then
				mv "$DESTDIR" "$WORKDIR/${_dir}"
				_dirty="yes"
			else
				rm -rf "$DESTDIR"
				_fail="yes"
				info_func_set ""
				return 1
			fi
		fi
	fi
	unset DESTDIR
	all_dirs="$all_dirs $_dir"
	_my_sections="$_my_sections $_dir"
	info_func_set ""
}

build_profile() {
	local _id _dir _spec
	_my_sections=""
	_dirty="no"
	_fail="no"

	profile_$PROFILE
	list_has $ARCH $arch || return 0

	info_func_set "build"

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

	info_func_set ""
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
	-*) usage; exit 1;;
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
done
echo "Images generated in $OUTDIR"
