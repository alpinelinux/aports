profile_base() {
	set_kernel_flavors "grsec"
	initfs_cmdline="modules=loop,squashfs,sd-mod,usb-storage quiet"
	set_initfs_features "ata base bootchart cdrom squashfs ext2 ext3 ext4 mmc raid scsi usb virtio"
	#grub_mod="disk part_msdos linux normal configfile search search_label efi_uga efi_gop fat iso9660 cat echo ls test true help"
	set_apks "alpine-base alpine-mirrors bkeymaps e2fsprogs network-extras libressl tzdata"

	feature_ssh
	feature_ntp
	overlays="base"
	hostname="alpine"
}
