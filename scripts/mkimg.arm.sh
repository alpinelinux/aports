build_rpi_blobs() {
	for i in raspberrypi-bootloader-common raspberrypi-bootloader; do
		apk fetch --root "$APKROOT" --quiet --stdout "$i" | tar -C "${DESTDIR}" -zx --strip=1 boot/
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
	MIN_IMG_SIZE=129 # minimum FAT16 partition size in MB to cross 4k cluster size boundary
	sync "$DESTDIR"
	local imgfile="${OUTDIR}/${output_filename%.gz}"
	local image_size=$(du -L -k -s "$DESTDIR" | awk '{print int(($1 + 8192) / 1024)}' )
	dd if=/dev/zero of="$imgfile" bs=1M count=$((1+( image_size > $MIN_IMG_SIZE ? image_size : $MIN_IMG_SIZE ))) # Min size +1MB for partition table
	echo 'start=2048, type=6, bootable' | sfdisk "$imgfile" # create partition table with FAT16 at standard 2048 sector (1MB offset)
	mkfs.vfat -n PIBOOT -F 16 --offset 2048 "$imgfile"
	mcopy -s -i "$imgfile"@@2048s "$DESTDIR"/* "$DESTDIR"/.alpine-release ::
	echo "Compressing $imgfile..."
	pigz -v -f -9 "$imgfile" || gzip -f -9 "$imgfile"
}

profile_rpiimg() {
	profile_rpi
	title="Raspberry Pi Disk Image"
	image_name="alpine-rpi"
	image_ext="img.gz"
}

