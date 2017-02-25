profile_virt() {
	profile_standard
	initfs_apks=""
	initfs_apks_flavored="linux"
	kernel_flavors="virtgrsec"
	kernel_cmdline="console=tty0 console=ttyS0,115200"
	syslinux_serial="0 115200"
}
