create_image_iso() {
	local ISO="${OUTDIR}/${output_filename}"
	local _isolinux
	local _efiboot

	if [ -e "${DESTDIR}/boot/syslinux/isolinux.bin" ]; then
		# isolinux enabled
		_isolinux="
			-isohybrid-mbr ${DESTDIR}/boot/syslinux/isohdpfx.bin
			-eltorito-boot boot/syslinux/isolinux.bin
			-eltorito-catalog boot/syslinux/boot.cat
			-no-emul-boot
			-boot-load-size 4
			-boot-info-table
			"
	fi
	if [ -e "${DESTDIR}/boot/grub/efiboot.img" ]; then
		# efi boot enabled
		if [ -z "$_isolinux" ]; then
			# efi boot only
			_efiboot="
				-efi-boot-part
				--efi-boot-image
				-e boot/grub/efiboot.img
				-no-emul-boot
				"
		else
			# hybrid isolinux+efi boot
			_efiboot="
				-eltorito-alt-boot
				-e boot/grub/efiboot.img
				-no-emul-boot
				-isohybrid-gpt-basdat
				"
		fi
	fi
	xorrisofs \
		-quiet \
		-output ${ISO} \
		-full-iso9660-filenames \
		-joliet \
		-rock \
		-volid "alpine-$PROFILE $RELEASE $ARCH" \
		$_isolinux \
		$_efiboot \
		-follow-links \
		${iso_opts} \
		${DESTDIR}
}
