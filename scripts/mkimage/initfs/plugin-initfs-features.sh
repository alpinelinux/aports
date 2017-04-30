plugin_initfs_features() {
	var_list_alias initfs_files
	var_list_alias initfs_hostcfg
	var_list_alias initfs_kpkgs
	var_list_alias initfs_modules
	var_list_alias initfs_pkgs
	var_list_alias initfs_files

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



###
##  Feature glob lists handling
###

# Usage: initfs_features_globs <type> <features>
initfs_features_globs() {
	local _type="$1"
	shift

	local f glob file fname
	for f in $@; do
		fname="$(type "_initfs_feature_${f//-/_}_${_type}")"
		if [ "$fname" = "${fname%%*shell function*}" ] ; then
			[ "$VERBOSE" ] && msg "No $_type list for feature '$f', skipping."
			continue
		fi
		[ "$VERBOSE" ] && msg "Selecting $_type for feature '$f'."
		_initfs_feature_${f//-/_}_${_type} | sed -e '/^$/d' -e '/^#/d' -e "s|^\.*/*||"
	done | sort -u
}

# Usage: initfs_features_kpkgs_globs <features>
initfs_features_kpkgs_globs() {
	initfs_features_globs "kpkgs" "$@"
}

# Usage: initfs_features_modules_globs <features>
initfs_features_modules_globs() {
	initfs_features_globs "modules" "$@"
}

# Usage: initfs_features_pkgs_globs <features>
initfs_features_pkgs_globs() {
	initfs_features_globs "pkgs" "$@"
}

# Usage: initfs_features_files_globs <features>
initfs_features_files_globs() {
	initfs_features_globs "files" "$@"
}

# Usage: initfs_features_files_globs <features>
initfs_features_hostcfg_globs() {
	initfs_features_globs "hostcfg" "$@"
}

