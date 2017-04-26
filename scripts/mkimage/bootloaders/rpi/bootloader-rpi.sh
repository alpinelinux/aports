bootloader_rpi() {
	bootloader_rpi_enabled="true"
}

build_bootloader_rpi_blobs() {
	local fw
	for fw in bootcode.bin fixup.dat start.elf $bootloader_rpi_fw_add ; do
		curl --remote-time https://raw.githubusercontent.com/raspberrypi/firmware/${rpi_firmware_commit}/boot/${fw} \
			--output "${DESTDIR}"/${fw} || return 1
	done
}

bootloader_rpi_gen_cmdline() {
	printf '%s  %s' "$(get_initfs_cmdline)" "$(get_kernel_cmdline)"
}

bootloader_rpi_gen_config() {
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

build_bootloader_rpi_config() {
	bootloader_rpi_gen_cmdline > "${DESTDIR}"/cmdline.txt
	bootloader_rpi_gen_config > "${DESTDIR}"/config.txt
}

section_bootloader_rpi_config() {
	[ "$bootloader_rpi_enabled" = "true" ] || return 0
	build_section bootloader_rpi_config $( (bootloader_rpi_gen_cmdline ; bootloader_rpi_gen_config) | checksum )
	build_section bootloader_rpi_blobs "$rpi_firmware_commit"
}
