plugin_initfss() {
	var_list_alias initfs_files
	var_list_alias initfs_modules

	var_list_alias initfs_features
	var_list_alias initfs_load_modules

	var_list_alias initfs_apks
	var_list_alias initfs_apks_flavored
	var_list_alias initfs_only_apks
	var_list_alias initfs_only_apks_flavored

	# initfs_cmdline
	return 0
}


get_initfs_cmdline() {
	_mod="$(get_initfs_load_modules_with_sep ',')"
	printf '%s' "${_mod:+modules=$_mod}${initfs_cmdline:+ $initfs_cmdline}"
}

append_initfs_cmdline() {
	initfs_cmdline="${initfs_cmdline}${1:+ $1}"
}

