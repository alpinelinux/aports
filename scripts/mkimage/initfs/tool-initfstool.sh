####
###  INITFSTOOL
###
###  Initramfs components build and staging tool.
####

# Tool: initfstool
# Commands: help stage restage unstage depmod mkmodsubset mkmodcpio mkmodcpiogz mkmodloop

# Usage: tool_initfstool
tool_initfstool() { return 0 ; }

# Usage: initfstool
initfstool() {
	info_prog_set "initfstool"

	local command
	command="$1"

	: "${OPT_apkroot_setup_cmdline:="--repositories-file /etc/apk/repositories --host-keys"}"
	case "$1" in
		stage) shift ; unset UNSTAGE RESTAGE ; initfstool_stage "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$HOSTNAME" "$@" ; return $? ;;
		restage) shift ; unset UNSTAGE ; RESTAGE="yes" ; initfstool_stage "${STAGING_ROOT}" ${OPT_arch:+"$OPT_arch"} "$@" ; return $? ;;
		unstage) shift ; unset RESTAGE ; UNSTAGE="yes" ; initfstool_stage "${STAGING_ROOT}" ${OPT_arch:+"$OPT_arch"} "$@" ; return $? ;;
		depmod) shift ; initfstool_depmod "${STAGING_ROOT}/${OPT_arch:-"$(_apk --print-arch)"}/merged-$1" "$1" $2 ; return $? ;;
		mkmodsubset) shift ; initfstool_mkmodsubset "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		mkmodcpio) shift ; initfstool_mkmodcpio "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		mkmodcpiogz) shift ; initfstool_mkmodcpiogz "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		mkmodloop) shift ; initfstool_mkmodloop "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		build-base) shift ; initfstool_build_base_cpiogz "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		build-apkfiles) shift ; initfstool_build_apkfiles_cpiogz "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		build-apkcfg) shift ; initfstool_build_apkcfg_cpiogz "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		build-hostcfg) shift ; initfstool_build_hostcfg_cpiogz "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		mkinitfs) shift ; mkinitfs "${STAGING_ROOT}" "${OPT_arch:-"$(_apk --print-arch)"}" "$@" ; return $? ;;
		help) initfstool_usage && return 0 ;;
		--*) warning "Unhandled global option '$1'!" ; return 1 ;;
		*) warning "Unknown command '$1'!" ; return 1 ;;
	esac
	return 1
}



# Print usage
# Usage: initfstool_usage
initfstool_usage() {
	initfstool_commands_usage
	multitool_opts_usage
	apkroot_opts_usage
}


# Usage: initfstool_commands_usage
initfstool_commands_usage() {
cat <<EOF
Usage: initfstool <global opts> <command> <command opts>

Commands:


EOF
}


# cpio helper wrapper, use within a subshell and change to source directory, then
# feed a diet of relative paths with find or other tool.
# Usage: ( cd <srcdir> && <generate file list cmd> | _initfs_cpio <additional cpio opts> > <output file> )
_initfs_cpiogz() {
	sort -u | cpio --quiet -o -H newc | gzip -9
}


initfstool_stage() {
	info_prog_set "initfstool-stage"
	info_func_set "stage"
	local stage_base="$1" _arch="$2" _name="$3" _krel="$4"
	shift 4
	local _ddir="$stage_base/$_arch/initfs/$_name"
	local _apkroot="$_ddir/apkroot"
	if [ "$UNSTAGE" ] ; then rm -rf "$_ddir" && return $?
	elif [ "$RESTAGE" ] ; then rm -rf "$_ddir" ; fi

	apkroottool setup "$_apkroot" "$_arch"
	apkroottool "$_apkroot" apk update
	apkroottool "$_apkroot" apk add --no-scripts "$(initfs_features_pkgs_globs "$@")"
	apkroottool deps "$_apkroot"
	[ "$_initfs_nomods" != "yes" ] && kerneltool stage "$_krel" "$(initfs_features_kpkgs_globs "$@")"
}

# Usage: mkinitfs_main <source dir> <temp dir> <outfile> <kernel version> <initfs-features...>
mkinitfs_main() {
	local _apkroot _tgt _out _krel _err
	_apkroot="${1%/}"
	_bdir="${2%/}"
	_out="${3%/}"
	_krel="${4}"
	shift 4
	local _ddir="$stage_base/$_arch/initfs/$_subname"
	local _apkroot="$_ddir/apkroot"

	initfs_file_root="${initfs_file_root:-/usr/share/mkinitfs}"
	initfs_src_fstab="${initfs_src_fstab:-$initfs_file_root/fstab}"
	initfs_src_init="${initfs_src_init:-$initfs_file_root/initramfs-init}"

	initfs_base_cpiogz="$_bdir/out/initfs-base.cpio.gz"
	initfs_hostcfg_cpiogz="$_bdir/out/initfs-hostcfg.cpio.gz"
	initfs_apkcfg_cpiogz="$_bdir/out/initfs-apkcfg.cpio.gz"
	initfs_apkfiles_cpiogz="$_bdir/out/initfs-apkfiles.cpio.gz"

	_apkroot="$_bdir/apkroot"

	_error=""
	umask 0022
	mkdir_is_writable "$_bdir/base"
	mkdir_is_writable "$_bdir/out"
	mkdir_is_writable "$_apkroot"
	initfs_build_base_cpiogz "$_bdir/base" > "$initfs_base_cpiogz" && _c="$initfs_base_cpiogz"
	initfs_build_hostcfg_cpiogz "${_hostcfg_root:-/}" > "$initfs_hostcfg_cpiogz" && _c="$_c $initfs_hostcfg_cpiogz"
	initfs_build_apkcfg_cpiogz "$_apkroot" > "$initfs_apkcfg_cpiogz" && _c="$_c $initfs_apkcfg_cpiogz" 
	initfs_build_apkfiles_cpiogz "$_apkroot" > "$initfs_apkfiles_cpiogz" && _c="$_c $initfs_apkfiles_cpiogz" || error="apkfiles"
	( set -- $(initfs_features_files_globs "$@") ; apkroottool subset-cpiogz "-" "$@" )

	if [ "$_mkinitfs_nomods" != "yes" ] ; then
		( 	set -- $(initfs_features_modules_globs "$@")
			kerneltool mkmodcpiogz "$_krel" "$@"
		) && _c="$_c initfs-modules-$krel.cpio.gz" || _error="modules"
	fi

	rm -f "$_out" || _error="output file cleanup"
	(cd "$_bdir" && set -- $_c && cat "$@" ) > "$_out" || _error="output file combining"

	[ "$_error" ] && warning "mkintfs: Building initfs $_error failed!" && return 1
	return 0
}


# Usage: initfs_build_base_cpiogz <work base dir> <features...>
initfs_build_base_cpiogz() {
	local _src _tgt
	_bdir="${1%/}"
	shift

	# TODO: initfstool - audit list of default directories in initfs-base.cpio.gz
	local i=
	for i in dev proc sys sbin bin run .modloop lib/modules media/cdrom etc/apk media/usb newroot; do
		mkdir_is_writable "$_bdir/base/$i" || ! warning "Could not create writable directory at '$_bdir/base/$i'!" || return 1
	done

	# copy init
	install -m755 "$initfs_src_init" "$_bdir/base/init" || return 1
	install -m644 "$initfs_src_fstab" "$_bdir/base/etc/fstab" || return 1

	(cd "$_bdir" && find | _initfs_cpiogz )
}

initfs_build_apkcfg_cpiogz() {
	local _src _ddir
	_apkroot="${1%/}"

	(cd "$_apkroot" && find etc/apk \( -name "arch" -o -name "repositories" -o -name "*.pub" \) -type f  | _initfs_cpiogz )
}

initfs_build_hostcfg_cpiogz() {
	local _src _tgt
	_src="${1%/}"
	shift 2

	(	cd "${_src:-/}" || ! warning "Could not change to source directory for hostcfg at '${_src:-/}'!" || return 1
		initfs_features_hostcfg_globs "$@" \
			| while IFS= read -r _line ; do
				set -- $_line
				for _file do
					[ -e "$_file" ] || ! msg "No file or directory matching glob '$_line'." || continue
					printf '%s\n' "$_file"
				done
			done | _initfs_cpiogz
	)
}


mkinitfs_wrapper_usage() {
	cat <<EOF
usage: mkinitfs [-nKklLh] [-b basedir] [-o outfile] [-t tempdir]
		[-c configfile] [-F features] [-P featuresdir]
		[-f fstab] [-i initfile] [kernelversion]
options:
	-b  prefix files and kernel modules with basedir
	-o  set another outfile
	-t  use tempdir when creating initramfs image
	-c  use configfile instead of default
	-F  use specified features
	-P  additional paths to find features
	-f  use fstab instead of default
	-i  use initfile as init instead of default
	-n  don't include kernel modules or firmware
	-K  copy also host keys to initramfs
	-k  keep tempdir
	-q  Quiet mode
	-l  only list files that would have been used
	-L  list available features
	-h  print this help

EOF
	exit 1
}

mkinitfs() {
	local _opt _val _src _tgt _out _kver _features _conf _keeptmp _pd _d _listfiles _listfeatures
	_mkinitfs_nomods="no"
	while [ "${1}" != "${1#-}" ] ; do
		case "$1" in
			'-b' ) _src="${2%/}" ; shift 2 ; continue ;;
			'-o' ) _out="$2" ; shift 2 ; continue ;;
			'-t' ) _tgt="${2%/}" ; shift 2 ; continue ;;
			'-c' ) _conf="$2" ; shift 2 ; continue ;;
			'-F' ) _features="$2" ; shift 2 ; continue ;;
			'-P' ) _pd="${_pd:+ $_pd}$2" ; shift 2 ;; # Load features from files/dirs.
			'-f' ) initfs_src_fstab="$2" ; shift 2 ; continue ;;
			'-i' ) initfs_src_init="$2" ; shift 2 ; continue ;;
			'-n' ) _mkinitfs_nomods="yes" ; shift ;; # Modules included by default
			'-K' ) shift ; _hostkeys="yes";;
			'-k' ) _keeptmp="yes" ; shift ;; # Cleaning up tmp enabled by default
			'-l' ) shift ; _listfiles="yes" ;;
			'-L' ) shift ; _listfeatures="yes" ;;
			'-h' ) shift ; mkinitfs_wrapper_usage ; exit 0 ;;
			'-q' ) shift ;;
			* ) shift ; mkinitfs_wrapper_usage ; exit 1 ;;

		esac
	done

	_kver="${1:-$(uname -r)}"
	shift

	[ "$_pd" ] && load_plugins "$_pd"

	_src="${_src:-/}"
	_tgt="${_tgt:-$(mktemp -dt mkinitfs.XXXXXX)}"
	_out="${_out:-${_kver#*-*-}}"


	if [ "$_conf" ] ; then
		file_is_readable "$_conf"
		local features
		. "$_conf"
		_features="${_features:+$_features }${features}"
		unset features
	fi



	if [ "$_listfeatures" = "yes" ] ; then printf '%s\n' $all_initfss ; exit 0 ; fi

	[ "$_features" ] || [ "$@" ] || ! warning "mkinitfs: Called with no features!" || return 1

	if [ "$_listfiles" = "yes" ] ; then
		mkinitfs_find_files "$_src" "$_features"
		mkinitfs_find_kmods "$_src" "$_kver" "$_features"
		mkinitfs_find_firmware "$_src" "$_kver" "$_features"
		exit 0
	fi

	dir_is_readable "$_src" || ! warning "mkinitfs: Could not read from source directory '$_src'"
	mkdir_is_writable "$_tgt" || ! warning "mkinitfs: Could create writable work directory '$_tgt'"

	mkdir_is_writable "${_out%/*?}" || ! warning "mkinitfs: Can not write to output directory '${_out%/*?}'"
	if [ -e "$_out" ] ; then
		rm -f "$_out"
		[ -e "$_out" ] && file_is_writable "${_out}" \
			|| ! warning "mkinitfs: Output file exists and can not be overwritten: '${_out}'"
	fi


	mkinitfs_main "$_src" "$_tgt" "$_out" "$_kver" $_features $@ || return 1

	[ "$_keeptmp" ] || rm -rf "$_tgt"

}


