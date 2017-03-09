profile_virt() {
	profile_standard
	clear_initfs_apks
	set_initfs_apks_flavored "linux"
	set_kernel_flavors "virtgrsec"
	append_kernel_cmdline "console=tty0 console=ttyS0,115200"
	syslinux_serial="0 115200"
}
