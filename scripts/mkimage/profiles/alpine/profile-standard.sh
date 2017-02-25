profile_standard() {
	profile_base
	image_ext="iso"
	arch="x86 x86_64"
	output_format="iso"
	kernel_cmdline="nomodeset"
	initfs_apks_flavored="$initfs_apks_flavored xtables-addons"
}
