plugin_initfs() {
	# initfs_cmdline
	# 
	var_list_alias initfs_features
	var_list_alias initfs_load_modules

	var_list_alias initfs_apks
	var_list_alias initfs_apks_flavored
	var_list_alias initfs_only_apks
	var_list_alias initfs_only_apks_flavored

	return 0
}


get_initfs_cmdline() {
	# Intentionally not quoted to clean up list!
	_mod="$(get_initfs_load_modules_with_sep ',')"
	printf '%s' "${_mod:+modules=$_mod}${initfs_cmdline:+ $initfs_cmdline}"
}

append_initfs_cmdline() {
	initfs_cmdline="${initfs_cmdline}${1:+ $1}"
}




plugin_kernels() {
	# kernel_cmdline
	return 0
}

get_kernel_cmdline() {
	printf '%s' "${kernel_cmdline}"
}

append_kernel_cmdline() {
	kernel_cmdline="${kernel_cmdline}${1:+ $1}"
}

section_kernels() {
	local _flavor _pkgs
	for _flavor in $(get_kernel_flavors); do
		#FIXME Should these really be hard-coded?
		var_list_set _pkgs "$(suffix_kernel_flavor "linux") linux-firmware"

		var_list_add _pkgs "$initfs_apks $initfs_only_apks"
		var_list_add _pkgs "$(suffix_kernel_flavor $_flavor $initfs_apks_flavored $initfs_only_apks_flavored)"

		local id=$( (echo "$initfs_features::$_hostkeys" ; _apk fetch --simulate alpine-base $_pkgs | sort -u ) | checksum)
		build_section kernel $ARCH $_flavor $id $_pkgs
	done
}

build_kernel() {
	local _flavor="$2"
	shift 3
	local _pkgs="$@"
	( update-kernel \
		$_hostkeys \
		${_abuild_pubkey:+--apk-pubkey $_abuild_pubkey} \
		--media \
		--flavor "$_flavor" \
		--arch "$ARCH" \
		--package "$_pkgs" \
		--feature "$(get_initfs_features_with_sep ',')" \
		--repositories-file "$APKROOT/etc/apk/repositories" \
		"$DESTDIR"
	) || warning "update-kernel failed!"

}
