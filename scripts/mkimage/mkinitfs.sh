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
. "$scriptdir/utils/utils-file.sh"
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
$(mkinitfs_old_usage)
known initfs features: $all_initfss

EOF
}


# load plugins from script dir and ~/.mkimage
info_prog_set "$scriptname:plugin-loader"

load_plugins "$scriptdir/initfs"
info_prog_set "$scriptname"


mkinitfs "$@"

