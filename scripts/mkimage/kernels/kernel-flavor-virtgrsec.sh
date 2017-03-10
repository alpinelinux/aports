kernel_flavor_virtgrsec() {
	flavor="virtgrsec"
	add_kernel_flavors "$flavor"
	flavor_suffix="-$flavor"
	kernel_suffix="-$flavor"
	return 0
}
