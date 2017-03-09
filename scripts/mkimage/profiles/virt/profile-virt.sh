profile_virt() {
	set_kernel_flavors "virtgrsec"
	profile_standard
	clear_initfs_apks
	set_initfs_apks_flavored "linux"
	append_kernel_cmdline "console=tty0 console=ttyS0,115200"
	syslinux_serial="0 115200"
}
