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
	case "$ARCH" in
	armhf)
		cat <<-EOF
		disable_splash=1
		boot_delay=0
		gpu_mem=256
		gpu_mem_256=64
		[pi0]
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
	aarch64)
		cat <<-EOF
		disable_splash=1
		boot_delay=0
		arm_control=0x200
		kernel=boot/vmlinuz-rpi
		initramfs boot/initramfs-rpi
		# uncomment line to enable serial on ttyS0 on rpi3
		# NOTE: This fixes the core_freq to 250Mhz
		# enable_uart=1
		EOF
	;;
	esac
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
	title="Raspberry Pi"
	desc="Includes Raspberry Pi kernel.
		Designed for RPI 1,2 and 3.
		And much more..."
	image_ext="tar.gz"
	arch="armhf"
	# check commit log for matching commit with current rpi kernel version at:
	# https://github.com/raspberrypi/firmware/tree/master
	rpi_firmware_commit="eeaaf5e2b5aee29f31e989c0dddd186fb68b2144"
	kernel_flavors="rpi rpi2"
	kernel_cmdline="dwc_otg.lpm_enable=0 console=tty1"
	initrd_features="base bootchart squashfs ext4 f2fs kms mmc raid scsi usb"
	apkovl="genapkovl-dhcp.sh"
	hostname="rpi"
	if [ "$ARCH" = "aarch64" ]; then
		arch="aarch64"
		kernel_flavors="rpi"
	fi
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
