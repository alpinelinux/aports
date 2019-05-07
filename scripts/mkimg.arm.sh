build_rpi_blobs() {
	apk fetch --quiet --stdout raspberrypi-bootloader | tar -C "${DESTDIR}" -zx --strip=1 boot/
}

rpi_gen_cmdline() {
	echo "modules=loop,squashfs,sd-mod,usb-storage quiet ${kernel_cmdline}"
}

rpi_gen_config() {
	cat <<-EOF
	# do not modify this file as it will be overwritten on upgrade.
	# create and/or modify usercfg.txt instead.
	# https://www.raspberrypi.org/documentation/configuration/config-txt
	EOF
	case "$ARCH" in
	armhf)
		cat <<-EOF
		[pi0]
		kernel=boot/vmlinuz-rpi
		initramfs boot/initramfs-rpi
		[pi0w]
		kernel=boot/vmlinuz-rpi
		initramfs boot/initramfs-rpi
		[pi1]
		kernel=boot/vmlinuz-rpi
		initramfs boot/initramfs-rpi
		[pi2]
		kernel=boot/vmlinuz-rpi2
		initramfs boot/initramfs-rpi2
		[pi3]
		kernel=boot/vmlinuz-rpi2
		initramfs boot/initramfs-rpi2
		[pi3+]
		kernel=boot/vmlinuz-rpi2
		initramfs boot/initramfs-rpi2
		[all]
		include usercfg.txt
		EOF
	;;
	armv7)
		cat <<-EOF
		[pi2]
		kernel=boot/vmlinuz-rpi2
		initramfs boot/initramfs-rpi2
		[pi3]
		kernel=boot/vmlinuz-rpi2
		initramfs boot/initramfs-rpi2
		[pi3+]
		kernel=boot/vmlinuz-rpi2
		initramfs boot/initramfs-rpi2
		[all]
		include usercfg.txt
		EOF
	;;
	aarch64)
		cat <<-EOF
		arm_control=0x200
		kernel=boot/vmlinuz-rpi
		initramfs boot/initramfs-rpi
		include usercfg.txt
		EOF
	;;
	esac
}

build_rpi_config() {
	rpi_gen_cmdline > "${DESTDIR}"/cmdline.txt
	rpi_gen_config > "${DESTDIR}"/config.txt
}

section_rpi_config() {
	[ "$hostname" = "rpi" ] || return 0
	build_section rpi_config $( (rpi_gen_cmdline ; rpi_gen_config) | checksum )
	build_section rpi_blobs
}

profile_rpi() {
	profile_base
	title="Raspberry Pi"
	desc="Includes Raspberry Pi kernel.
		Designed for RPI 1, 2 and 3.
		And much more..."
	image_ext="tar.gz"
	arch="aarch64 armhf armv7"
	kernel_flavors="rpi"
	case "$ARCH" in
		armhf) kernel_flavors="rpi rpi2";;
		armv7) kernel_flavors="rpi2";;
	esac
	kernel_cmdline="console=tty1"
	initfs_features="base squashfs mmc usb kms dhcp https"
	hostname="rpi"
	grub_mod=
}

build_uboot() {
	set -x
	# FIXME: Fix apk-tools to extract packages directly
	local pkg pkgs="$(apk fetch  --simulate --root "$APKROOT" --recursive u-boot-all | sed -ne "s/^Downloading \([^0-9.]*\)\-.*$/\1/p")"
	for pkg in $pkgs; do
		[ "$pkg" = "u-boot-all" ] || apk fetch --root "$APKROOT" --stdout $pkg | tar -C "$DESTDIR" -xz usr
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
	title="Generic ARM"
	desc="Has default ARM kernel.
		Includes the uboot bootloader.
		Supports armhf and aarch64."
	image_ext="tar.gz"
	arch="aarch64 armhf armv7"
	kernel_flavors="vanilla"
	kernel_addons="xtables-addons"
	initfs_features="base bootchart squashfs ext4 kms mmc raid scsi usb"
	apkovl="genapkovl-dhcp.sh"
	hostname="alpine"
	uboot_install="yes"
}
