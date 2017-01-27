build_rpi_blobs() {
	local fw
	for fw in bootcode.bin fixup.dat start.elf ; do
		curl --remote-time https://raw.githubusercontent.com/raspberrypi/firmware/${rpi_firmware_commit}/boot/${fw} \
			--output "${DESTDIR}"/${fw} || return 1
	done
}

rpi_gen_cmdline() {
	echo "modules=loop,squashfs,sd-mod,usb-storage quiet ${kernel_cmdline}"
}

rpi_gen_config() {
	cat <<EOF
disable_splash=1
boot_delay=0
gpu_mem=256
gpu_mem_256=64
[pi0]
kernel=boot/vmlinuz-rpi
initramfs boot/initramfs-rpi 0x08000000
[pi1]
kernel=boot/vmlinuz-rpi
initramfs boot/initramfs-rpi 0x08000000
[pi2]
kernel=boot/vmlinuz-rpi2
initramfs boot/initramfs-rpi2 0x08000000
[pi3]
kernel=boot/vmlinuz-rpi2
initramfs boot/initramfs-rpi2 0x08000000
[all]
include usercfg.txt
EOF
}

build_rpi_config() {
	rpi_gen_cmdline > "${DESTDIR}"/cmdline.txt
	rpi_gen_config > "${DESTDIR}"/config.txt
}

section_rpi_config() {
	[ -n "$rpi_firmware_commit" ] || return 0
	build_section rpi_config $( (rpi_gen_cmdline ; rpi_gen_config) | checksum )
	build_section rpi_blobs "$rpi_firmware_commit"
}

profile_rpi() {
	profile_base
	image_ext="tar.gz"
	arch="armhf"
	rpi_firmware_commit="debe2d29bbc3df84f74672fae47f3a52fd0d40f1"
	kernel_flavors="rpi rpi2"
	kernel_cmdline="dwc_otg.lpm_enable=0 console=ttyAMA0,115200 console=tty1"
	initrd_features="base bootchart squashfs ext2 ext3 ext4 f2fs kms mmc raid scsi usb"
	apkovl="genapkovl-dhcp.sh"
	hostname="rpi"
	image_ext="tar.gz"
}

build_uboot() {
	# FIXME: Fix apk-tools to extract packages directly
	local pkg pkgs="$(apk fetch  --simulate --root /tmp/timo/apkroot-armhf/ --recursive u-boot-all | sed -ne "s/^Downloading \([^0-9.]*\)\-.*$/\1/p")"
	for pkg in $pkgs; do
		[ "$pkg" == "u-boot-all" ] || apk fetch --root "$APKROOT" --stdout $pkg | tar -C "$DESTDIR" -xz usr
	done
	mkdir -p "$DESTDIR"/u-boot
	mv "$DESTDIR"/usr/sbin/update-u-boot "$DESTDIR"/usr/share/u-boot/* "$DESTDIR"/u-boot
	rm -rf "$DESTDIR"/usr
}

section_uboot() {
	[ -n "$uboot_install" ] || return 0
	build_section uboot $ARCH $(apk fetch --root "$APKROOT" --simulate --recursive u-boot-all | sort | checksum)
}

profile_uboot() {
	profile_base
	image_ext="tar.gz"
	arch="aarch64 armhf armv7"
	case "$ARCH" in
	aarch64)
		kernel_flavors="vanilla"
		kernel_addons=
		;;
	*)
		kernel_flavors="grsec"
		kernel_addons="xtables-addons"
		;;
	esac
	initfs_features="base bootchart squashfs ext2 ext3 ext4 kms mmc raid scsi usb"
	apkovl="genapkovl-dhcp.sh"
	hostname="alpine"
	uboot_install="yes"
}
