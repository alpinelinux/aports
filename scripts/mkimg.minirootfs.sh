section_minirootfs() {
	return 0
}

create_image_rootfs() {
	local _script=$(readlink -f "$scriptdir/genrootfs.sh")
	local output_file="$(readlink -f ${OUTDIR:-.})/$output_filename"

	(cd "$OUTDIR"; fakeroot "$_script" -k "$APKROOT"/etc/apk/keys \
		-r "$APKROOT"/etc/apk/repositories \
		-o "$output_file" \
		-a $ARCH \
		$rootfs_apks)
}

profile_minirootfs() {
	image_ext=tar.gz
	output_format=rootfs
	arch="x86 x86_64 armhf aarch64 ppc64le"
	rootfs_apks="busybox alpine-baselayout alpine-keys apk-tools libc-utils"
}
