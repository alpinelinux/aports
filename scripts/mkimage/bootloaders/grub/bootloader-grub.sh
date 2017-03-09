#FIXME No non-efi grub bootloader defined. Should this be implemented for supporting grub on non-efi systems?
bootloader_grub() {
	warning "bootloader_grub not currently implemented, please using bootloader_grubefi instead!"
	bootloader_grubefi "$@"
}


# GgrubEFI bootloader plugin - works on x86_64/aarch64 efi systems only.
bootloader_grubefi() {
	list_has $ARCH "x86_64 aarch64" || return 0

	if [ "$1" = "disabled" ] ; then
		bootloader_grubefi_enabled="false"
	elif [ "$1" = "enabled" ] || [ "$bootloader_grubefi_enabled" != "false" ] ; then
		bootloader_grubefi_enabled="true"
		bootloader_grub_cfg
	fi
}

# grub.cfg ootloader plugin - sets grub_cfg_file variable to default of boot/grub/grub.cfg
bootloader_grub_cfg() {
	grub_cfg_file="${grub_cfg_file:-boot/grub/grub.cfg}"
}


# Grub configuration generator - runs as long as variable $grub_cfg_file is not empty.
section_grub_cfg() {
	[ "$grub_cfg_file" ] || return 0
	build_section grub_cfg "$grub_cfg_file" $(grub_cfg_generate | checksum)
}

grub_cfg_generate() {
	local _f _kf _tab
	tab=$'\t' # Used in heredoc because leading tabs are trimmed with <<- operator.
	echo "set timeout=2"
	for _f in $kernel_flavors; do
		_kf=""
		[ "$_f" = vanilla ] || _kf=-$_f

		cat <<- EOF

			menuentry "Linux $_f" {
			${_tab}linux	/boot/vmlinuz$_kf $(get_initfs_cmdline) $(get_kernel_cmdline)
			${_tab}initrd	/boot/initramfs-$_f
			}
		EOF
	done
}

build_grub_cfg() {
	local grub_cfg="$1"
	mkdir -p "${DESTDIR}/$(dirname $grub_cfg)"
	grub_cfg_generate > "${DESTDIR}"/$grub_cfg
}


# GrubEFI boot support.
section_grubefi() {
	[ -n "$grub_mod" ] || return 0
	[ "$bootloader_grubefi_enabled" = "true" ] || return 0

	local _format _efi
	case "$ARCH" in
	x86_64)
		_format="x86_64-efi"
		_efi="bootx64.efi"
		;;
	aarch64)
		_format="arm64-efi"
		_efi="bootaa64.efi"
		;;
	*)
		return 0
		;;
	esac

	build_section grubefi_img $_format $_efi $(grub_early_cfg_generate | checksum)
}

grub_early_cfg_generate() {
	cat <<- EOF
	search --no-floppy --set=root --label "alpine-$PROFILE $RELEASE $ARCH"
	set prefix=(\$root)/boot/grub
	EOF
}

build_grubefi_img() {
	local _format="$1"
	local _efi="$2"
	local _tmpdir="$WORKDIR/efiboot.$3"

	# Prepare grub-efi bootloader
	mkdir -p "$_tmpdir/efi/boot"
	grub_early_cfg_generate > "$_tmpdir"/grub_early.cfg
	grub-mkimage \
		--config="$_tmpdir"/grub_early.cfg \
		--prefix="/boot/grub" \
		--output="$_tmpdir/efi/boot/$_efi" \
		--format="$_format" \
		--compression="xz" \
		$grub_mod

	# Create the EFI image
	# mkdosfs and mkfs.vfat are busybox applets which failed to create a proper image
	# use dosfstools mkfs.fat instead
	mkdir -p ${DESTDIR}/boot/grub/
	dd if=/dev/zero of=${DESTDIR}/boot/grub/efiboot.img bs=1K count=1440
	mkfs.fat -F 12 ${DESTDIR}/boot/grub/efiboot.img
	mcopy -s -i ${DESTDIR}/boot/grub/efiboot.img $_tmpdir/efi ::
}
