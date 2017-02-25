profile_virt() {
	profile_standard
	kernel_flavors="virtgrsec"
	kernel_cmdline="console=tty0 console=ttyS0,115200"
	syslinux_serial="0 115200"
}
