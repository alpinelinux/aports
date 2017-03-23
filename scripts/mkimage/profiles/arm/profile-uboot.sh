profile_uboot() {
	set_archs "aarch64 armhf armv7"
	case "$ARCH" in
	aarch64)
		set_kernel_flavors "vanilla"
		;;
	*)
		set_kernel_flavors "grsec"
		;;
	esac
	profile_base

	set_initfs_features "base bootchart squashfs ext2 ext3 ext4 kms mmc raid scsi usb"

	feature_dhcp "client=autostart"
	hostname="alpine"

	bootloader_uboot
	image_ext="tar.gz"
}
