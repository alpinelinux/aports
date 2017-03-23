profile_netinstall() {
	set_kernel_flavors_if_empty "grsec"
	add_initfs_load_modules "loop squashfs sd-mod usb-storage"
	set_initfs_features "ata-all base drivers-cdrom fs-squashfs fs-ext2 fs-ext3 fs-ext4 drivers-mmc md-raid scsi-all drivers-usb virt-virtio"
	#grub_mod="disk part_msdos linux normal configfile search search_label efi_uga efi_gop fat iso9660 cat echo ls test true help"
	set_apks "alpine-base alpine-mirrors bkeymaps e2fsprogs network-extras libressl tzdata"

	feature_ssh
	feature_ntp
	add_overlays "base"
	set_hostname "alpine"
}
