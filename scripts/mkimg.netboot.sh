create_image_netboot() {
	rm -rf "${OUTDIR}"/netboot-$RELEASE "${OUTDIR}"/netboot
	cp -aL "${DESTDIR}"/boot "${OUTDIR}"/netboot-$RELEASE
	ln -s netboot-$RELEASE "${OUTDIR}"/netboot
	tar -C "${DESTDIR}" -chzf ${OUTDIR}/${output_filename} boot/
}

profile_netboot() {
	title="Netboot"
	desc="Kernel, initramfs and modloop for
		netboot.
		"
	arch="aarch64 armhf ppc64le x86 x86_64 s390x"
	kernel_cmdline="nomodeset"
	kernel_flavors="vanilla"
	apks=""
	initfs_features="base network squashfs usb virtio"
	output_format="netboot"
	image_ext="tar.gz"
	case "$ARCH" in
	x86_64) kernel_flavors="$kernel_flavors virt";;
	s390x) initfs_features="$initfs_features dasd_mod qeth";;
	esac
}

