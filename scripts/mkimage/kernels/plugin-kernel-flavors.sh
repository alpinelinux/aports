
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


# Usage: 'add_kernel_flavor <flavor> <flavor suffix> <kernel suffix>'
add_kernel_flavor() {
	local _flavor_suffix="${2--$1}"
	local _kernel_suffix="${3--$_flavor_suffix}"

	setvar "_kernel_flavor_$1_flavor_suffix" "$_flavor_suffix"
	setvar "_kernel_flavor_$1_kernel_suffix" "$_kernel_suffix"

	add_kernel_flavors "$1"

	kernel_flavor="${1}"
}

get_kernel_flavor_suffix() {
	getvar "_kernel_flavor_$1_flavor_suffix"
}

get_kernel_flavor_kernel_suffix() {
	getvar "_kernel_flavor_$1_kernel_suffix"
}
