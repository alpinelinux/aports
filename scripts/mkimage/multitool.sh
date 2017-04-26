#!/bin/sh
set -e

toolname="${0##*/}" && toolname="${toolname%.sh}"
scriptrealpath="$(realpath "$0")"
scriptname="${scriptrealpath##*/}"
scriptrealdir="$(dirname "$scriptrealpath")"

scriptdir="${scriptrealdir}"

# Include utilities:
. "$scriptdir/utils/utils-basic.sh"
. "$scriptdir/utils/utils-info.sh"
. "$scriptdir/utils/utils-list.sh"
. "$scriptdir/utils/utils-file.sh"
. "$scriptdir/utils/utils-search.sh"
. "$scriptdir/utils/utils-plugin-loader.sh"
. "$scriptdir/utils/utils-checksum.sh"
. "$scriptdir/utils/utils-apk.sh"
. "$scriptdir/utils/utils-fkrt.sh"

# Check for quiet flag and set global 'QUIET=yes' before running anything that generates output. Skipped in main option parser.
# Let verbose override quiet before setting verbose, but quiet acts directly.
_co="$@" && for _i in $_co ; do case "$_i" in -q|--quiet) QUIET="yes" ; VERBOSE="" ;; -v|--verbose) [ "$QUIET" = "yes" ] || VERBOSE="yes" ; QUIET="" ;; --) break ;; esac ; done

default_colors
info_prog_set "$scriptname"

multitool_usage() { multitool_opts_usage ; } 

multitool_opts_usage() {
cat <<EOF
Global options:
	--quiet|-q			Enable quiet mode.
	--verbose|-v			Increase verbosity.
	--apk-cmd			Override APK command.
	--arch				Override arch.
	--staging-root			Override staging root.
	--install-root			Override install root.

EOF
}

apkroot_opts_usage() {
cat <<EOF
apkroot options:
	--cache-dir			Set apk cache directory for apkroot.
	--repository|--repo		Add apk repository to apkroot repos.
	--repositories-file|--repo-file	Add apk repos from file to apkroot repos.
	--key-file|--key		Add key in file to apkroot keys dir.
	--keys-dir			Add keys in dir to apkroot keys dir.
	--host-keys			Copy host keys to apkroot keys dir.
	--no-host-keys			Don't copy host keys.

EOF
}


# load plugins from script dir and ~/.mkimage
info_prog_set "$scriptname:plugin-loader (tools)"
load_plugins "$scriptdir" "tools"
info_prog_set "$scriptname"


[ "$toolname" = "multitool" ] && toolname="$1" && shift


case "$(type $toolname)" in
	*"shell function") TOOL="$toolname" ;;
	*) error "Unknown tool '$toolname'!" ; return 1 ;;
esac


case "$TOOL" in
	apkroottool) : ;;
	kerneltool)
		load_plugins "$scriptdir/initfs" 
		load_plugins "$scriptdir/kernels"
		load_plugins "$scriptdir/archs"

		. "$scriptdir/kernels/tool-kerneltool.sh"
		. "$scriptdir/kernels/tool-kerneltool-apk.sh"
		. "$scriptdir/kernels/tool-kerneltool-kbuild.sh"
		;;
	mkinitfs)	
		load_plugins "$scriptdir/initfs"
		. "$scriptdir/initfs/plugin-mkinitfs.sh"
		;;

esac



	OPT_apkroot_setup_cmdline=""
	while [ $# -gt 0 ] ; do

		# Global options that may apply to any tool.
		case "$1" in
			--quiet|-q) shift ; continue ;;
			--verbose|-v) shift ; continue ;;
			--apk-cmd) OPT_apk_cmd="$2" ; shift 2 ; continue ;;
			--arch) OPT_arch="$2"; shift 2 ; continue ;;
			--staging-root) OPT_staging_root="$2" ; shift 2 ; continue ;;
			--install-root) OPT_install_root="$2" ; shift 2 ; continue ;;
		esac

		# Options for tools that call apkroot_setup
		case "$1" in
			# Options to pass to apkroot_setup
			--cache-dir) OPT_apkroot_setup_cmdline="${OPT_apkroot_setup_cmdline:+"$OPT_apkroot_setup_cmdline "}--cache-dir $2" ; shift 2 ; continue ;;
			--repository|--repo) OPT_apkroot_setup_cmdline="${OPT_apkroot_setup_cmdline:+"$OPT_apkroot_setup_cmdline "}--repository $2" ; shift 2 ; continue ;;
			--repositories-file|--repo-file) OPT_apkroot_setup_cmdline="${OPT_apkroot_setup_cmdline:+"$OPT_apkroot_setup_cmdline "}--repositories-file $2" ; shift 2 ; continue ;;
			--key-file|--key) OPT_apkroot_setup_cmdline="${OPT_apkroot_setup_cmdline:+"$OPT_apkroot_setup_cmdline "}--key-file $2" ; shift 2 ; continue ;;
			--keys-dir) OPT_apkroot_setup_cmdline="${OPT_apkroot_setup_cmdline:+"$OPT_apkroot_setup_cmdline "}--keys-dir $2" ; shift 2 ; continue ;;
			--host-keys) OPT_apkroot_setup_cmdline="${OPT_apkroot_setup_cmdline:+"$OPT_apkroot_setup_cmdline "}--host-keys" ; shift ; continue ;;
			--no-host-keys) OPT_apkroot_setup_cmdline="${OPT_apkroot_setup_cmdline:+"$OPT_apkroot_setup_cmdline "}--no-host-keys" ; shift ; continue ;;
		esac
		break
		#case "$1" in
		case "$1" in -*|--*) warning "Unknown global option '$1'!" ; shift ; continue ;; *) break ;; esac
	done

	# Setup our apk tool:
	[ "$APK" ] || _apk_init ${OPT_apk_cmd:+"$OPT_apk_cmd"}
	# Determine our staging root
	: "${mkalpine_staging_root:=/var/cache/mkalpine/staging}"
	STAGING_ROOT="${OPT_staging_root:-$mkalpine_staging_root}"

#set -- $_args

"$TOOL" "$@"
