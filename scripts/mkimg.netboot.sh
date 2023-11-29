create_image_netboot() {
	rm -rf "${OUTDIR}"/netboot-$RELEASE "${OUTDIR}"/netboot
	cp -aL "${DESTDIR}"/boot "${OUTDIR}"/netboot-$RELEASE
	ln -s netboot-$RELEASE "${OUTDIR}"/netboot
	# let webserver read initramfs
	chmod a+r "${OUTDIR}"/netboot-$RELEASE/*
	tar -C "${DESTDIR}" -chzf ${OUTDIR}/${output_filename} boot/
}

profile_netboot() {
	title="Netboot"
	desc="Kernel, initramfs and modloop for
		netboot.
		"
	arch="aarch64 armhf armv7 ppc64le x86 x86_64 s390x"
	case "$ARCH" in
		armhf) kernel_flavors="rpi";;
		armv7) kernel_flavors="lts rpi";;
		aarch64) kernel_flavors="lts virt rpi";;
		x86_64) kernel_flavors="lts virt";;
		*) kernel_flavors="lts";;
	esac
	case "$ARCH" in
		s390x) initfs_features="base network squashfs usb dasd_mod qeth zfcp";;
		*) initfs_features="base network squashfs usb virtio"
	esac
	modloop_sign=yes
	apks=""
	output_format="netboot"
	image_ext="tar.gz"
}

