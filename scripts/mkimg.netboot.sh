create_image_netboot() {
	rm -rf "${OUTDIR}"/netboot-$RELEASE
	cp -a "${DESTDIR}"/boot "${OUTDIR}"/netboot-$RELEASE
	tar -C "${DESTDIR}" -chzf ${OUTDIR}/${output_filename} boot/
}

profile_netboot() {
	title="Netboot"
	desc="Kernel, initramfs and modloop for
		netboot.
		"
	arch="x86_64 s390x"
	kernel_cmdline="nomodeset"
	kernel_flavors="vanilla"
	apks=""
	initfs_features="ata base bootchart squashfs ext2 ext3 ext4 mmc network scsi usb virtio"
	output_format="netboot"
	image_ext="tar.gz"
	case "$ARCH" in
	x86_64) kernel_flavors="$kernel_flavors virt";;
	s390x) initfs_features="$initfs_features dasd_mod qeth";;
	esac
}

