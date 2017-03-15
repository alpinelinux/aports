profile_standard() {
	# Set our target archs
	set_archs "x86 x86_64"

	# Load parent profile(s)
	profile_base

	# Load features
	feature_xtables_addons

	# Configure kernel
	append_kernel_cmdline "nomodeset"

	# Configure output
	imagetype_iso
}
