####
###  kerneltool - apk handling tools.
####



# Print arch/krel pair for kernel apk using usr/share/kernel/<flavor>/kernel.release .
# Usage: kerneltool_apk_get_arch_kernel_release <kernel apk file>
kerneltool_apk_get_arch_kernel_release() {
	local a="$1" && file_is_readable "$a" || ! warning "Could not read '$a'!" || return 1
	printf '%s/%s' \
		$(tar -x -O -f "$a" .PKGINFO 2> /dev/null | grep -e '^arch = ' | cut -d' ' -f 3) \
		$(tar -x -O -f "$a" usr/share/kernel 2> /dev/null)
}



###
##  Extract kernel artifacts from standard kernel packages.
###


# Usage: kerneltool_apk_extract_kernel <arch> <kernel release> <apkfile> <destination dir>
kerneltool_apk_extract_kernel() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	apk_extract "$_apkfile" "$_ddir" boot usr/share/kernel || ! warning "Could not extract kernel from '$_apkfile' to '$_ddir'!" || return 1
}


# Usage: kerneltool_apk_extract_modules <arch> <kernel release> <apkfile> <destination dir>
kerneltool_apk_extract_modules() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	apk_extract "$_apkfile" "$_ddir" lib/modules || ! warning "Could not extract modules from '$_apkfile' to '$_ddir'!" || return 1
}


# Usage: kerneltool_apk_extract_firmware <arch> <kernel release> <apkfile> <destination dir>
kerneltool_apk_extract_firmware() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	apk_extract "$_apkfile" "$_ddir" lib/firmware || ! warning "Could not extract firmware from '$_apkfile' to '$_ddir'!" || return 1
}


# Usage: kerneltool_apk_extract_dtbs <arch> <kernel release> <apkfile> <destination dir>
kerneltool_apk_extract_dtbs() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	apk_extract "$_apkfile" "$_ddir" usr/lib || ! warning "Could not extract dtbs from '$_apkfile' to '$_ddir'!" || return 1
}


# Usage: kerneltool_apk_extract_headers <arch> <kernel release> <apkfile> <destination dir>
kerneltool_apk_extract_headers() {
	local _arch="$1" _krel="$2" _apkfile="$3" _ddir="$4"
	apk_extract "$_apkfile" "$_ddir" usr/include || ! warning "Could not extract kernel headers from '$_apkfile' to '$_ddir'!" || return 1
}

