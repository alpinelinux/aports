build_rpi_blobs() {
	for i in raspberrypi-bootloader-common raspberrypi-bootloader; do
		apk fetch --root "$APKROOT" --quiet --stdout "$i" | tar -C "${DESTDIR}" -zx --strip=1 boot/ || return 1
	done
}

rpi_gen_cmdline() {
	echo "modules=loop,squashfs,sd-mod,usb-storage quiet ${kernel_cmdline}"
}

rpi_gen_config() {
	local arm_64bit=0
	case "$ARCH" in
		aarch64) arm_64bit=1;;
	esac
	cat <<-EOF
		# do not modify this file as it will be overwritten on upgrade.
		# create and/or modify usercfg.txt instead.
		# https://www.raspberrypi.com/documentation/computers/config_txt.html

		kernel=boot/vmlinuz-rpi
		initramfs boot/initramfs-rpi
		arm_64bit=$arm_64bit
		include usercfg.txt
	EOF
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
	desc="First generation Pis including Zero/W (armhf).
		Pi 2 to Pi 3+ generations (armv7).
		Pi 3 to Pi 5 generations (aarch64)."
	image_ext="tar.gz"
	arch="aarch64 armhf armv7"
	kernel_flavors="rpi"
	kernel_cmdline="console=tty1"
	initfs_features="base squashfs mmc usb kms dhcp https"
	hostname="rpi"
	grub_mod=
}

create_image_imggz() {
	sync "$DESTDIR"
	local image_size=$(du -L -k -s "$DESTDIR" | awk '{print $1 + 8192}' )
	local imgfile="${OUTDIR}/${output_filename%.gz}"
	dd if=/dev/zero of="$imgfile" bs=1M count=$(( image_size / 1024 ))
	mformat -i "$imgfile" -N 0 ::
	mcopy -s -i "$imgfile" "$DESTDIR"/* "$DESTDIR"/.alpine-release ::
	echo "Compressing $imgfile..."
	pigz -v -f -9 "$imgfile" || gzip -f -9 "$imgfile"
}

profile_rpiimg() {
	profile_rpi
	title="Raspberry Pi Disk Image"
	image_name="alpine-rpi"
	image_ext="img.gz"
}

build_uboot() {
	# FIXME: Fix apk-tools to extract packages directly
	local pkg pkgs="$(apk fetch  --simulate --root "$APKROOT" --recursive u-boot-all | sed -ne "s/^Downloading \(.*\)\-[0-9].*$/\1/p")"
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
		Supports armv7 and aarch64."
	image_ext="tar.gz"
	arch="aarch64 armv7"
	kernel_flavors="lts"
	kernel_addons="xtables-addons"
	initfs_features="base bootchart ext4 kms mmc nvme raid scsi squashfs usb"
	apkovl="genapkovl-dhcp.sh"
	hostname="alpine"
	uboot_install="yes"
}
