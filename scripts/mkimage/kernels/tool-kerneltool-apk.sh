##
## kerneltool - apk handling tools.
##

# Print arch/krel pair for kernel apk using usr/share/kernel/<flavor>/kernel.release .
# Usage: kerneltool_apk_get_arch_kernel_release <kernel apk file>
kerneltool_apk_get_arch_kernel_release() {
	local a="$1" && file_is_readable "$a" || ! warning "Could not read '$a'!" || return 1
	printf '%s/%s' \
		$(tar -x -O -f "$a" .PKGINFO 2> /dev/null | grep -e '^arch = ' | cut -d' ' -f 3) \
		$(tar -x -O -f "$a" usr/share/kernel 2> /dev/null)
}

# Fetch apk for specified package to specified destination dir.
# Usage: kerneltool_apk_fetch <arch> <kernel release> <package> <destination dir>
kerneltool_apk_fetch() {
	local _arch="$1" _krel="$2" _pkg="$3" _ddir="$4"
	local _pkgfile="$_ddir/$_pkg.apk"

	local _a_
	for _a_ in 1 2 ; do
		file_exists "$_pkgfile" || _apk fetch -o "$_ddir" "${_pkg%-*-*}"
		_apk verify "$_pkgfile" && break
		rm -f "$_pkgfile"
		[ $_a_ -eq 2 ] && return 1
	done

}

# Extract packaged kernel related files.
# Usage: kerneltool_apk_extract <arch> <kernel release> <apk file> <destination dir> [<path to extract>...]
kerneltool_apk_extract() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	shift 4

	_archpkgver="$(apk_get_apk_arch_package_version "$_apkfile")"
	local _apkarch="${_archpkgver%%/*}"
	[ "$_arch" != "$_apkarch" ] && warning "Arch mismatch detected: '$_arch' requested but '$_apkarch' found in '$_apkfile'!" && return 1
	file_is_readable "$_apkfile" || ! warning "Could not read '$_apkfile' to extract!" || return 1
	_apk verify ${VERBOSE:--q} "$_apkfile" || ! warning "Could not verify '$_apkfile'!" || return 1

	_found="$( tar -tz -f "$_apkfile" | sed -n -E $([ "$1" ] && printf '-e s|^(%s).*|\\1|gp ' $@ || printf '1,$ p') | sort -u )"
	! [ "$_found" ] && msg "No $([ "$1" ] && printf ''\''%s'\'' ' $@ || printf 'files ')found in '$_apkfile', nothing extracted." && return 0
 
	mkdir_is_writable "$_ddir" || ! warning "Could not create output directory '$_ddir' to extract '$_apkfile'!" || return 1
	tar -C "$_ddir" -x -f "$_apkfile" $_found || ! warning "Could not extract files${_found+" in '$_found'"} from '$_apkfile'!" || return 1
	msg "Extracted $(printf ''\''%s'\'' ' $_found)from '$_apkfile.'"

	return 0
}


# Using standard kernel packages
kerneltool_apk_extract_kernel() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	kerneltool_apk_extract "$_arch" "$_krel" "$_apkfile" "$_ddir" boot usr/share/kernel || ! warning "Could not extract kernel from '$_apkfile' to '$_ddir'!" || return 1
}


kerneltool_apk_extract_modules() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	kerneltool_apk_extract "$_arch" "$_krel" "$_apkfile" "$_ddir" lib/modules || ! warning "Could not extract modules from '$_apkfile' to '$_ddir'!" || return 1
}


kerneltool_apk_extract_firmware() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	kerneltool_apk_extract "$_arch" "$_krel" "$_apkfile" "$_ddir" lib/firmware || ! warning "Could not extract firmware from '$_apkfile' to '$_ddir'!" || return 1
}


kerneltool_apk_extract_dtbs() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	kerneltool_apk_extract "$_arch" "$_krel" "$_apkfile" "$_ddir" usr/lib || ! warning "Could not extract dtbs from '$_apkfile' to '$_ddir'!" || return 1
}


kerneltool_apk_extract_headers() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	kerneltool_apk_extract "$_arch" "$_krel" "$_apkfile" "$_ddir" usr/include || ! warning "Could not extract kernel headers from '$_apkfile' to '$_ddir'!" || return 1
}

