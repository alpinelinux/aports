profile_standard() {
	# Load parent profile(s)
	profile_base

	# Set architecture and kernel flavor
	set_arch "x86 x86_64"

	# Load features
	feature_xtables_addons

	# Configure kernel
	kernel_cmdline="nomodeset"

	# Configure output
	image_ext="iso"
	output_format="iso"
}
