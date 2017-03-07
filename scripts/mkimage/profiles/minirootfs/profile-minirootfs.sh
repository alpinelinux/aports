section_minirootfs() {
	return 0
}

profile_minirootfs() {
	image_ext=tar.gz
	output_format=rootfs
	set_archs "x86 x86_64 armhf aarch64 ppc64le"
	set_rootfs_apks "busybox alpine-baselayout alpine-keys apk-tools libc-utils"
}
