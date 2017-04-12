#!/bin/sh

# apk add \
#	abuild apk-tools alpine-conf busybox fakeroot syslinux xorriso
#	(for efi:) mtools dosfstools grub-efi

# FIXME: clean workdir out of unneeded sections
# FIXME: --release: cp/mv images to REPODIR/$ARCH/releases/
# FIXME: --update-latest: rewrite latest-releases.yaml with this build

set -e

toolname="${0##*/}" && toolname="${toolname%.sh}"
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
. "$scriptdir/utils/utils-apk.sh"

# Check for quiet flag and set global 'QUIET=yes' before running anything that generates output. Skipped in main option parser.
_co="$@" && for _i in $_co ; do case "$_i" in -q|--quiet) QUIET="yes" ; break ;; --) break ;; esac ; done

default_colors
info_prog_set "$scriptname"


checksum() {
	sha1sum | cut -f 1 -d ' '
}


# load plugins from script dir and ~/.mkimage
info_prog_set "$scriptname:plugin-loader (tools)"
load_plugins "$scriptdir" "tools"
info_prog_set "$scriptname"


[ "$toolname" = "multitool" ] && toolname="$1" && shift


case "$(type tool_$toolname)" in
	*"shell function") TOOL="$toolname" ;;
	*) error "Unknown tool '$toolname'!" ; return 1 ;;
esac


case "$TOOL" in
	kerneltool)
		load_plugins "$scriptdir/initfs" 
		load_plugins "$scriptdir/kernels"
		load_plugins "$scriptdir/archs"

		. "$scriptdir/kernels/tool-kerneltool.sh"
		. "$scriptdir/kernels/tool-kerneltool-apk.sh"
		. "$scriptdir/kernels/tool-kerneltool-kbuild.sh"
		
		;;

esac



	OPT_apkroot_tool_cmdline=""
	while [ $# -gt 0 ] ; do

		# Options that apply to all tools.
		case "$1" in
			--quiet|-q) shift ; continue ;;
			--apk-cmd) OPT_apk_cmd="$2" ; shift 2 ; continue ;;
			--arch) OPT_arch="$2"; shift 2 ; continue ;;
			--staging-root) OPT_staging_root="$2" ; shift 2 ; continue ;;
			--install-root) OPT_install_root="$2" ; shift 2 ; continue ;;
		esac

		# Options for tools that call apkroot_tool
		case "$1" in
			# Options to pass to apkroot_tool
			--cache-dir) OPT_apkroot_tool_cmdline="${OPT_apkroot_tool_cmdline:+"$OPT_apkroot_tool_cmdline "}--cache-dir $2" ; shift 2 ; continue ;;
			--repository|--repo) OPT_apkroot_tool_cmdline="${OPT_apkroot_tool_cmdline:+"$OPT_apkroot_tool_cmdline "}--repository $2" ; shift 2 ; continue ;;
			--repositories-file|--repo-file) OPT_apkroot_tool_cmdline="${OPT_apkroot_tool_cmdline:+"$OPT_apkroot_tool_cmdline "}--repositories-file $2" ; shift 2 ; continue ;;
			--key-file|--key) OPT_apkroot_tool_cmdline="${OPT_apkroot_tool_cmdline:+"$OPT_apkroot_tool_cmdline "}--key-file $2" ; shift 2 ; continue ;;
			--keys-dir) OPT_apkroot_tool_cmdline="${OPT_apkroot_tool_cmdline:+"$OPT_apkroot_tool_cmdline "}--keys-dir $2" ; shift 2 ; continue ;;
			--host-keys) OPT_apkroot_tool_cmdline="${OPT_apkroot_tool_cmdline:+"$OPT_apkroot_tool_cmdline "}--host-keys" ; shift ; continue ;;
			--no-host-keys) OPT_apkroot_tool_cmdline="${OPT_apkroot_tool_cmdline:+"$OPT_apkroot_tool_cmdline "}--no-host-keys" ; shift ; continue ;;
		esac

		case "$1" in
			--) _args="${args:+$args }$@" ; shift $# ; break ;;
			*) _args="${_args:+$_args }$1" ; shift ; continue;;
		esac
	done

	# Setup our apk tool:
	[ "$APK" ] || _apk_init ${OPT_apk_cmd:+"$OPT_apk_cmd"}
	# Determine our staging root
	: "${mkalpine_staging_root:=/var/cache/mkalpine/staging}"
	STAGING_ROOT="${OPT_staging_root:-$mkalpine_staging_root}"


"$TOOL" $_args


