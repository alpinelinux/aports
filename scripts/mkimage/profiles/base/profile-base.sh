build_kernel() {
	local _flavor="$2"
	shift 3
	local _pkgs="$@"
	update-kernel \
		$_hostkeys \
		${_abuild_pubkey:+--apk-pubkey $_abuild_pubkey} \
		--media \
		--flavor "$_flavor" \
		--arch "$ARCH" \
		--package "$_pkgs" \
		--feature "$initfs_features" \
		--repositories-file "$APKROOT/etc/apk/repositories" \
		"$DESTDIR"
}

add_flavor() {
	local _f _a _s=""
	for _f in $kernel_flavors; do
		for _a in $@; do
			_s="$s $_a-$_f"
		done
	done
	echo $_s
}

section_kernels() {
	local _f _a _pkgs
	for _f in $kernel_flavors; do
		#FIXME Should these really be hard-coded?
		_pkgs="linux-$_f linux-firmware"

		for _a in $initfs_apks $initfs_only_apks; do
			_pkgs="$_pkgs $_a"
		done

		for _a in $initfs_apks_flavored $initfs_only_apks_flavored; do
			_pkgs="$_pkgs $(add_flavor $_a)"
		done

		local id=$( (echo "$initfs_features::$_hostkeys" ; apk fetch --root "$APKROOT" --simulate alpine-base $_pkgs | sort) | checksum)
		build_section kernel $ARCH $_f $id $_pkgs
	done
}

build_apks() {
	local _apksdir="$DESTDIR/apks"
	local _archdir="$_apksdir/$ARCH"
	mkdir -p "$_archdir"

	apk fetch --root "$APKROOT" --link --recursive --output "$_archdir" $apks
	if ! ls "$_archdir"/*.apk >& /dev/null; then
		return 1
	fi

	apk index \
		--description "$RELEASE" \
		--rewrite-arch "$ARCH" \
		--index "$_archdir"/APKINDEX.tar.gz \
		--output "$_archdir"/APKINDEX.tar.gz \
		"$_archdir"/*.apk
	abuild-sign "$_archdir"/APKINDEX.tar.gz
	touch "$_apksdir/.boot_repository"
}

section_apks() {
	# Build list of all required apks
	[ -n "$apks_flavored" ] && apks="$apks $(add_flavor $apks_flavored)"
	[ -n "$initfs_apks" ] && apks="$apks $initfs_apks"
	[ -n "$initfs_apks_flavored" ] && apks="$apks $(add_flavor $initfs_apks_flavored)"
	[ -n "$apks" ] || return 0

	build_section apks $ARCH $(apk fetch --root "$APKROOT" --simulate --recursive $apks | sort | checksum)
}

build_apkovl() {
	local _host="$1" _script=
	msg "Generating $_host.apkovl.tar.gz"
	for _script in "$PWD"/"$apkovl" $HOME/.mkimage/$apkovl \
		$(readlink -f "$scriptdir/$apkovl"); do

		if [ -f "$_script" ]; then
			break
		fi
	done
	[ -n "$_script" ] || die "could not find $apkovl"
	(cd "$DESTDIR"; fakeroot "$_script" "$_host")
}

section_apkovl() {
	[ -n "$apkovl" -a -n "$hostname" ] || return 0
	build_section apkovl $hostname $(checksum < "$apkovl")
}

build_syslinux() {
	local _fn
	mkdir -p "$DESTDIR"/boot/syslinux
	apk fetch --root "$APKROOT" --stdout syslinux | tar -C "$DESTDIR" -xz usr/share/syslinux
	for _fn in isohdpfx.bin isolinux.bin ldlinux.c32 libutil.c32 libcom32.c32 mboot.c32; do
		mv "$DESTDIR"/usr/share/syslinux/$_fn "$DESTDIR"/boot/syslinux/$_fn || return 1
	done
	rm -rf "$DESTDIR"/usr
}

section_syslinux() {
	[ "$ARCH" = x86 -o "$ARCH" = x86_64 ] || return 0
	[ "$output_format" = "iso" ] || return 0
	build_section syslinux $(apk fetch --root "$APKROOT" --simulate syslinux | sort | checksum)
}

syslinux_gen_config() {
	[ -z "$syslinux_serial" ] || echo "SERIAL $syslinux_serial"
	echo "TIMEOUT ${syslinux_timeout:-20}"
	echo "PROMPT ${syslinux_prompt:-1}"
	echo "DEFAULT ${kernel_flavors%% *}"

	local _f _kf
	for _f in $kernel_flavors; do
		_kf=""
		[ "$_f" = vanilla ] || _kf=-$_f

		if [ ! "${xen_enabled}" = "true" ]; then
			cat <<- EOF

			LABEL $_f
				MENU LABEL Linux $_f
				KERNEL /boot/vmlinuz$_kf
				INITRD /boot/initramfs-$_f
				DEVICETREEDIR /boot/dtbs
				APPEND $initfs_cmdline $kernel_cmdline
			EOF
		else
			cat <<- EOF

			LABEL $_f
				MENU LABEL Xen/Linux $_f
				KERNEL /boot/syslinux/mboot.c32
				APPEND /boot/xen.gz ${xen_params} --- /boot/vmlinuz$_kf $initfs_cmdline $kernel_cmdline --- /boot/initramfs-$_f
			EOF
		fi
	done
}

grub_gen_config() {
	local _f _kf
	echo "set timeout=2"
	for _f in $kernel_flavors; do
		_kf=""
		[ "$_f" = vanilla ] || _kf=-$_f

		cat <<- EOF
		
		menuentry "Linux $_f" {
			linux	/boot/vmlinuz$_kf $initfs_cmdline $kernel_cmdline
			initrd	/boot/initramfs-$_f
		}
		EOF
	done
}

build_syslinux_cfg() {
	local syslinux_cfg="$1"
	mkdir -p "${DESTDIR}/$(dirname $syslinux_cfg)"
	syslinux_gen_config > "${DESTDIR}"/$syslinux_cfg
}

section_syslinux_cfg() {
	syslinux_cfg=""
	if [ "$ARCH" = x86 -o "$ARCH" = x86_64 ]; then
		[ ! "$output_format" = "iso" ] || syslinux_cfg="boot/syslinux/syslinux.cfg"
	fi
	[ ! -n "$uboot_install" ] || syslinux_cfg="extlinux/extlinux.conf"
	[ -n "$syslinux_cfg" ] || return 0
	build_section syslinux_cfg $syslinux_cfg $(syslinux_gen_config | checksum)
}

build_grub_cfg() {
	local grub_cfg="$1"
	mkdir -p "${DESTDIR}/$(dirname $grub_cfg)"
	grub_gen_config > "${DESTDIR}"/$grub_cfg
}

grub_gen_earlyconf() {
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
	grub_gen_earlyconf > "$_tmpdir"/grub_early.cfg
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

section_grubefi() {
	[ -n "$grub_mod" ] || return 0
	[ "$output_format" = "iso" ] || return 0

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

	build_section grub_cfg boot/grub/grub.cfg $(grub_gen_config | checksum)
	build_section grubefi_img $_format $_efi $(grub_gen_earlyconf | checksum)
}

profile_base() {
	kernel_flavors="grsec"
	initfs_cmdline="modules=loop,squashfs,sd-mod,usb-storage quiet"
	initfs_features="ata base bootchart cdrom squashfs ext2 ext3 ext4 mmc raid scsi usb virtio"
	#grub_mod="disk part_msdos linux normal configfile search search_label efi_uga efi_gop fat iso9660 cat echo ls test true help"
	apks="alpine-base alpine-mirrors bkeymaps chrony e2fsprogs network-extras libressl openssh tzdata"
	apkovl=
	hostname="alpine"
}
