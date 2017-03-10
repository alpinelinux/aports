kernel_flavor_vanilla() {
	flavor="vanilla"
	add_kernel_flavors "$flavor"
	flavor_suffix="-$flavor"
	kernel_suffix=""
	return 0
}
