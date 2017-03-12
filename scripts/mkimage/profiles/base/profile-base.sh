# Base profile - Normal alpine-base root filesystem.
#   Rootfs includes full alpine-base package and mirrors, binary keymaps, and timezone data.

profile_base() {

	# Load any additional options and packages required for specified base type.
	if [ "$profile_base_type" ] ; then
		local pfunc="$(type _profile_base_make_$profile_base_type)"
		if [ "${pfunc}" = "${pfunc%%*shell function}" ] ; then
			warning "Specified non-existent base type '$profile_base_type' - using 'default'!"
			profile_base_type="default"
		fi
	else profile_base_type="default" ; fi
	
	_profile_base_make_$profile_base_type

	# Default to include grsec flavored kernel if none previously set.
	set_kernel_flavors_if_empty "grsec"

	# Load the bootable skeleton profile
	profile_skel_bootable

	add_initfs_features "keymaps"
	# Add our base filesystem, keymaps, ssl/crypto tools, and timezone data
	add_rootfs_apks "alpine-base bkeymaps libressl tzdata"
	add_apks "alpine-mirrors"

	# Add the base overlay
	add_overlays "base"

	# Include ntp and ssh features
	feature_ntp
	feature_ssh autostart

}


# Base types --
# TODO: These probably need to be moved to a hardware config and out of the main profiles.

_profile_base_make_default(){ 
	add_initfs_load_modules "sd_mod usb-storage"
	add_initfs_features "ata base cdrom squashfs ext2 ext3 ext4 mmc raid scsi usb virtio"
	add_apks "e2fsprogs network-extras"
}

_profile_base_make_host() {
	add_initfs_load_modules "sd_mod usb-storage"
	add_initfs_features "scsi-basic ext2 usb"
	add_apks "e2fsprogs network-extras"
}

_profile_base_make_box() {
	_profile_base_make_host
	iadd_initfs_load_modules "ahci nvme cdrom vfat ext3 ext4"
	add_initfs_features "ata-ahci nvme cdrom fat ext3 ext4 network"
}

_profile_base_make_workstation() {
	_profile_base_make_box
	add_initfs_features "ata-sata scsi-drivers-sas"
}

_profile_base_make_pc() {
	_profile_base_make_box
	add_initfs_features "ata-sata"
}

_profile_base_make_all_kitchen_sink() {
	_profile_base_make_box
	add_initfs_features "9p ata bootchart btrfs cdrom cramfs cryptsetup f2fs floppy gfs2 iscsi iscsi-target jfs keymap kms kvm-amd kvm-intel lvm mmc nbd nvme ocfs2 raid reiserfs scsi ubifs virt-host virtio xfs zfs"
}

_profile_base_make_embedded() {
	_profile_base_make_host
	add_initfs_load_modules "sd_mod mmc ext2"
}

_profile_base_make_virtio() {

	# Default to include virtgrsec flavored kernel if none previously set.
	set_kernel_flavors_if_empty "virtgrsec"

	add_initfs_load_modules "scsi_mod sr_mod sd_mod cdrom virtio_scsi virtio_net"
	add_initfs_features "scsi-base cdrom virtio-guest-base ext2 ext3 ext4"
	#add_initfs_apks_flavored "linux"
	add_rootfs_apks "bridge vlan"
}


