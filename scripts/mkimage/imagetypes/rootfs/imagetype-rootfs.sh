imagetype_rootfs() {
	return 0
}

create_image_rootfs() {
	local _script=$(readlink -f "$scriptdir/imagetypes/rootfs/genrootfs.sh")
	local output_file="$(readlink -f ${OUTDIR:-.})/$output_filename"

	(cd "$OUTDIR"; fakeroot "$_script" -k "$APKROOT"/etc/apk/keys \
		-r "$APKROOT"/etc/apk/repositories \
		-o "$output_file" \
		-a $ARCH \
		$rootfs_apks)
}
