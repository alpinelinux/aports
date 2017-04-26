##
## kerneltool - kbuild build directory tools.
##

#
# Tools for querying the build directory.
#

# Get the current configured kernel build arch from .config and translate it to a system arch
kerneltool_kbuild_query_arch() {
	local _bdir="$1"
	file_is_readable "$_bdir/.config" || ! warning "Please configure and build your kernel at '$_bdir' before installing." || return 1
	local _karch="$(head -4 "$_bdir/.config" | sed -n -E -e 's|^# Linux/([-_[:alnum:]]+)[[:space:]].*|\1|' )"
	[ "$_karch" ] || ! warning "Could not determine configured kernel arch in build directory '$_bdir'!" || return 1
	local _arch="$(kerneltool_get_arch_from_karch "$_karch" )"
	[ "$_arch" ] || ! warning "Could not determine system arch based on configured kernel arch of '$_karch' in build directory '$_bdir'!" || return 1
	printf '%s' $_arch
}

# Print result of make kernelrelease/kernelversion/image_name from build directory.
# Usage: kerneltool_kbuild_query_(kernelrelease|kernelversion|image_name) <arch> <build dir>
kerneltool_kbuild_query_kernelrelease() { local _arch="$1" _bdir="$2" ; make -C "$_bdir" ARCH="$(kerneltool_get_karch_from_arch "$_arch")" -s kernelrelease || return $? ; }
kerneltool_kbuild_query_kernelversion() { local _arch="$1" _bdir="$2" ; make -C "$_bdir" ARCH="$(kerneltool_get_karch_from_arch "$_arch")" -s kernelversion || return $? ; }
kerneltool_kbuild_query_image_name() { local _arch="$1" _bdir="$2" ; make -C "$_bdir" ARCH="$(kerneltool_get_karch_from_arch "$_arch")" -s image_name || return $? ; }

# Print the current configured kernel build arch / kernel release from build dir.
# Usage: kerneltool_kbuild_query_arch_kernel_release <build dir>
kerneltool_kbuild_query_arch_kernel_release() {
	local _bdir="$1" _arch _krel

	_arch="$(kerneltool_kbuild_query_arch "$_bdir" )"
	[ "$_arch" ] || ! warning "Could not determine configured arch for kernel in '$_bdir'!" || return 1
	_krel="$(kerneltool_kbuild_query_kernelrelease "$_arch" "$_bdir")"
	[ "$_krel" ] || ! warning "Could not determine kernel version with 'make -s kernelrelease' in '$_bdir'." || return 1

	printf '%s/%s' "$_arch" "$_krel"
}


###
##  Kbuild targets (and a fake target and script for kernel_install)
###


# Print name of make target to build kernel image.
# Usage: get_kernel_make_kernel_target <arch> <kernel release> <build dir>
kerneltool_kbuild_make_kernel_target() {
	local _arch="$1" _krel="$2" _bdir="$3" _ddir="$4"
	local _target="$kernel_make_kernel_target"
	: "${_target:="$(getvar arch_${_arch}_kernel_make_kernel_target)"}"
	: "${_target:="$(kerneltool_kbuild_query_image_name "$_arch" "$_bdir")"}"
 	[ "$_target" ] || ! warning "No \$kernel_make_kernel_target or \$arch_${_arch}_kernel_make_kernel_target set, nor found with 'make -s image_name' in '$_ddir'!" || return 1
	printf '%s' "${_target}"
	return 0
}
kerneltool_kbuild_make_kernel_opts() { printf '%s' "${kernel_make_kernel_opts:-"$(getvar arch_$1_kernel_make_kernel_opts)"}" ; }

# No kerneltool_kbuild_make_kernel_install_*() because we copy kernel image with our own fake target.
# Usage: kerneltool_kbuild_make_kernel_install <arch> <kernel release> <build dir> <dest dir>
kerneltool_kbuild_make_kernel_install() {
	local _arch="$1" _krel="$2" _bdir="$3" _ddir="$4"
	shift 4

	local _tgt _img _err
	_tgt="$(kerneltool_kbuild_make_kernel_target "$_arch")"
	_img="$(kerneltool_kbuild_query_image_name "$_arch" "$_bdir")"

	file_is_readable "$_bdir/$_img" || kerneltool_make -q -s "$_tgt" || _err=$?
	if [ $_err -gt 1 ] ; then
		warning "Can not find make target '$_tgt' to ensure image is built!"
		return 1
	elif [ $_err -eq  1 ] ; then
		warning "Kernel image make target '$_tgt' is out of date! Please build kernel before installing."
		return 1
	fi

	mkdir_is_writable "$_ddir/boot" || ! warning "Can not create output directory for kernel at '$_ddir/boot'!" || return 1
	file_is_readable "$_bdir/$_img" && cp "$_bdir/$_img" "$_ddir/boot/$_img-$_krel" || ! warning "Can not copy kernel image from '$_bdir/$_img' to '$_ddir/boot/$_img-$krel'!" || return 1
	file_is_readable "$_bdir/System.map" && cp "$_bdir/$_img" "$_ddir/boot/System.map-$_krel" || ! warning "Can not copy '$_bdir/System.map' to '$_ddir/boot/System.map-$krel'!" || return 1
}


# Targets and options to build and install modules:
kerneltool_kbuild_make_modules_target() { printf '%s' "${kernel_make_modules_target:-modules}" ; }
kerneltool_kbuild_make_modules_install_target() { printf '%s' "${kernel_make_modules_install_target:-modules_install}" ; }
kerneltool_kbuild_make_modules_install_opts() { local _ddir="$4" ; printf '%s' "${kernel_make_modules_install_opts:-INSTALL_MOD_PATH="$_ddir${kernel_modules_install_path:-/}"}" ; }

# Targets and options to build and install firmware:
kerneltool_kbuild_make_firmware_target() { printf '%s' "${kernel_make_firmware_target:-firmware}" ; }
kerneltool_kbuild_make_firmware_install_target() { printf '%s' "${kernel_make_firmware_install_target:-firmware_install}" ; }
kerneltool_kbuild_make_firmware_install_opts() { local _ddir="$4" ; printf '%s' "${kernel_make_firmware_install_opts:-${kernel_firmware_install_path:-INSTALL_MOD_PATH="$_ddir${kernel_modules_install_path:-/}"}}" ; }

# Targets and options to build and install dtbs:
kerneltool_kbuild_make_dtbs_target() { printf '%s' "${kernel_make_dtbs_target:-dtbs}" ; }
kerneltool_kbuild_make_dtbs_install_target() { printf '%s' "${kernel_make_dtbs_install_target:-dtbs_install}" ; }
kerneltool_kbuild_make_dtbs_install_opts() {
	local _arch="$1" _krel="$2" _bdir="$3" _ddir="$4"

	local _archopts="$(getvar arch_${_arch}_kernel_make_dtbs_install_opts)"
	local _archpath="$(getvar arch_${_arch}_kernel_make_dtbs_install_path)"

	if [ "$kernel_make_dtbs_install_opts" ] ; then printf '%s' "$kernel_make_dtbs_install_opts"
	elif [ "$_archopts" ] ; then printf '%s' "$_archopts"
	elif [ "$kernel_dtbs_install_path" ] ; then printf 'INSTALL_DTBS_PATH="%s"' "$_ddir$kernel_dtbs_install_path"
	elif [ "${_archpath-/usr/lib/linux-$_krel}" ] ; then printf 'INSTALL_DTBS_PATH="%s"' "$_ddir$kernel_dtbs_install_path"
	else printf 'INSTALL_DTBS_PATH="%s"' "$_ddir/usr/lib/linux-$_krel" ; fi
}

# Targets and options to build and install headers:
kerneltool_kbuild_make_headers_target() { printf '%s' "${kernel_make_headers_target:-headers_check}" ; }
kerneltool_kbuild_make_headers_install_target() { printf '%s' "${kernel_make_headers_install_target:-headers_install}" ; }
kerneltool_kbuild_make_headers_install_opts() { local _ddir="$4" ; printf '%s' "${kernel_make_headers_install_opts:-INSTALL_HDR_PATH="$_ddir${kernel_headers_install_path:-/usr}"}" ; }




#
# Tools for building kbuild targets
#

# Usage: kerneltool_kbuild_make <arch> <build dir> <dest dir> <make command line...>
kerneltool_kbuild_make() {
	local _karch="$(kerneltool_get_karch_from_arch $1)" _bdir="$2"
	shift 2
	make -C "$_bdir" ARCH="$_karch" $@ || return $?
}

# Usage: kerneltool_kbuild_get_make_target <arch> <kernel release> <build dir> <dest dir>
kerneltool_kbuild_get_make_target() {
	local _arch="$1" _krel="$2" _bdir="$3" _ddir="$4" _tgt="$5"
	shift 5

	local f _target
	f="kerneltool_kbuild_make_${_tgt}_target"
	case "$(type $f)" in
		*function*) _target="$($f "$_arch" "$_krel" "$_bdir" "$_ddir")" ;;
		*) warning "No '$f' function found!" ; return 1 ;;
	esac
	printf '%s' "$_target"
}

# Usage: kerneltool_kbuild_get_make_opts <arch> <kernel release> <build dir> <dest dir>
kerneltool_kbuild_get_make_opts() {
	local _arch="$1" _krel="$2" _bdir="$3" _ddir="$4" _tgt="$5"
	shift 5

	local _opts o
	
	o="kerneltool_kbuild_make_${_tgt}_opts"
	case "$(type $o)" in *function*) _opts="$($o "$_arch" "$_krel" "$_bdir" "$_ddir")" ;; esac
	
	[ "$kernel_make_opts$_opts" ] && printf '%s ' $kernel_make_opts $_opts
}

# Usage: kerneltool_kbuild_make_target <arch> <kernel release string> <build dir> <dest dir> <make targets (get_kernel_make_*_target) ...>
kerneltool_kbuild_make_target() {
	local _arch="$1" _krel="$2" _bdir="$3" _ddir="$4"
	shift 4

	local i
	for i do
		case "$i" in install|kernel_install) kerneltool_make_kernel_install "$_arch" "$_krel" "$_bdir" "$_ddir" ; continue ; esac
		kerneltool_make "$_arch" "$_bdir" \
			"$(kerneltool_get_make_opts "$_arch" "$_krel" "$_bdir" "$_ddir" "$_tgt")" \
			"$(kerneltool_get_make_target "$_arch" "$_krel" "$_bdir" "$_ddir" "$_tgt")" || ! warning "kernltool_make failed!"
	done
}

