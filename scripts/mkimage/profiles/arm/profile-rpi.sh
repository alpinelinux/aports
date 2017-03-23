# rpi base profile
# TODO: profile_rpi - Make profile options configurable!
profile_rpi() {
	set_kernel_flavors "rpi rpi2"
	profile_base
	set_archs "armhf"
	set_initfs_features "base bootchart squashfs ext2 ext3 ext4 f2fs kms mmc raid scsi usb"
	rpi_firmware_commit="debe2d29bbc3df84f74672fae47f3a52fd0d40f1"
	append_kernel_cmdline "dwc_otg.lpm_enable=0 console=ttyAMA0,115200 console=tty1"
	bootloader_rpi
	feature_dhcp "client=autostart"
	set_hostname "rpi"
	set_image_ext "tar.gz"
}
