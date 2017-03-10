
plugin_kernel_flavors() {
	var_list_alias kernel_flavors
	return 0
}

# Helpers to append kernel-flavor suffixes to all specified roots.
# One flavor:
suffix_kernel_flavor() {
	local _flavor="$1"
	shift

	list_add_suffix "-${_flavor}" $@
}
# All specified flavors:
suffix_kernel_flavors() {
	local _flavor
	for _flavor in $(get_kernel_flavors); do
		suffix_kernel_flavor $_flavor $@
	done
}

