profile_rpi() {
	profile_base
	set_arch "armhf"
	set_kernel_flavors "rpi rpi2"
	set_initfs_features "base bootchart squashfs ext2 ext3 ext4 f2fs kms mmc raid scsi usb"
	rpi_firmware_commit="debe2d29bbc3df84f74672fae47f3a52fd0d40f1"
	kernel_cmdline="dwc_otg.lpm_enable=0 console=ttyAMA0,115200 console=tty1"
	bootloader_rpi
	add_overlays "dhcp_client"
	hostname="rpi"
	image_ext="tar.gz"
}
