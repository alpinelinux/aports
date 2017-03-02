profile_uboot() {
	profile_base
	image_ext="tar.gz"
	set_arch "aarch64 armhf armv7"
	case "$ARCH" in
	aarch64)
		set_kernel_flavors "vanilla"
		;;
	*)
		set_kernel_flavors "grsec"
		;;
	esac
	set_initfs_features "base bootchart squashfs ext2 ext3 ext4 kms mmc raid scsi usb"
	feature_xtables_addons
	apkovl="genapkovl-dhcp.sh"
	hostname="alpine"
	bootloader_uboot
}
