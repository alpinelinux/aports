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
	$(kernel_flavors_is_empty) && return 0
	build_all_kernels
}

