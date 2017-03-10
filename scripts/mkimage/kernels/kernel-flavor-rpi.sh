kernel_flavor_rpi() {
	flavor="rpi"
	add_kernel_flavors "$flavor"
	flavor_suffix="-$flavor"
	kernel_suffix="-$flavor"
	return 0
}

kernel_flavor_rpi2() {
	flavor="rpi2"
	add_kernel_flavors "$flavor"
	flavor_suffix="-$flavor"
	kernel_suffix="-$flavor"
	return 0
}
