

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

