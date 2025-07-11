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
	title="Mini root filesystem"
	desc="Minimal root filesystem.
		For use in containers
		and minimal chroots."
	image_ext=tar.gz
	output_format=rootfs
	arch="$ARCH"  # allow any arch
	rootfs_apks="busybox alpine-baselayout alpine-keys alpine-release apk-tools libc-utils"
}
