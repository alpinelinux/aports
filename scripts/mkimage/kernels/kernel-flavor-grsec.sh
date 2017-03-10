kernel_flavor_grsec() {
	flavor="grsec"
	add_kernel_flavors "$flavor"
	flavor_suffix="-$flavor"
	kernel_suffix="-$flavor"
	return 0
}
