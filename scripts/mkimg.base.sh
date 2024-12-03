build_kernel() {
	local _flavor="$2" _modloopsign= _add
	shift 3
	local _pkgs="$@"
	[ "$modloop_sign" = "yes" ] && _modloopsign="--modloopsign"
	update-kernel \
		$_hostkeys \
		${_abuild_pubkey:+--apk-pubkey $_abuild_pubkey} \
		$_modloopsign \
		--media \
		--keys-dir "$APKROOT/etc/apk/keys" \
		--flavor "$_flavor" \
		--arch "$ARCH" \
		--package "$_pkgs" \
		--feature "$initfs_features" \
		--modloopfw "$modloopfw" \
		--repositories-file "$APKROOT/etc/apk/repositories" \
		"$DESTDIR" \
		|| return 1
	for _add in $boot_addons; do
		apk fetch --root "$APKROOT" --quiet --stdout $_add | tar -C "${DESTDIR}" -zx boot/
	done
}

section_kernels() {
	local _f _a _pkgs
	for _f in $kernel_flavors; do
		_pkgs="linux-$_f linux-firmware wireless-regdb $modloop_addons"
		for _a in $kernel_addons; do
			_pkgs="$_pkgs $_a-$_f"
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
		--root "$APKROOT" \
		--description "$RELEASE" \
		--rewrite-arch "$ARCH" \
		--index "$_archdir"/APKINDEX.tar.gz \
		--output "$_archdir"/APKINDEX.tar.gz \
		"$_archdir"/*.apk
	abuild-sign "$_archdir"/APKINDEX.tar.gz
	touch "$_apksdir/.boot_repository"
}

section_apks() {
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
	echo "TIMEOUT ${syslinux_timeout:-10}"
	echo "PROMPT ${syslinux_prompt:-1}"
	echo "DEFAULT ${kernel_flavors%% *}"

	local _f _p _initrd
	for _f in $kernel_flavors; do
		if [ -z "${xen_params+set}" ]; then
			_initrd="/boot/initramfs-$_f"
			for _p in $initrd_ucode; do
				_initrd="$_p,$_initrd"
			done

			cat <<- EOF

			LABEL $_f
				MENU LABEL Linux $_f
				KERNEL /boot/vmlinuz-$_f
				INITRD $_initrd
				FDTDIR /boot/dtbs-$_f
				APPEND $initfs_cmdline $kernel_cmdline
			EOF
		else
			cat <<- EOF

			LABEL $_f
				MENU LABEL Xen/Linux $_f
				KERNEL /boot/syslinux/mboot.c32
				APPEND /boot/xen.gz ${xen_params} --- /boot/vmlinuz-$_f $initfs_cmdline $kernel_cmdline --- /boot/initramfs-$_f
			EOF
		fi
	done
}

grub_gen_config() {
	local _f _p _initrd
	echo "set timeout=1"
	for _f in $kernel_flavors; do
		if [ -z "${xen_params+set}" ]; then
			_initrd="/boot/initramfs-$_f"
			for _p in $initrd_ucode; do
				_initrd="$_p $_initrd"
			done

			cat <<- EOF

			menuentry "Linux $_f" {
				linux	/boot/vmlinuz-$_f $initfs_cmdline $kernel_cmdline
				initrd	$_initrd
			}
			EOF
		else
			cat <<- EOF

			menuentry "Xen/Linux $_f" {
				multiboot2	/boot/xen.gz ${xen_params}
				module2		/boot/vmlinuz-$_f $initfs_cmdline $kernel_cmdline
				module2		/boot/initramfs-$_f
			}
			EOF
		fi
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

gen_volid() {
	printf "%s" "alpine-${profile_abbrev:-$PROFILE} ${RELEASE%_rc*} $ARCH" | cut -c1-32
}

grub_gen_earlyconf() {
	cat <<- EOF
	search --no-floppy --set=root --label "$(gen_volid)"
	set prefix=(\$root)/boot/grub
	EOF
}

build_grub_efi() {
	local _format="$1"
	local _efi="$2"

	apk fetch --root "$APKROOT" --quiet --stdout grub-efi | tar -C "$WORKDIR" -zx usr/lib/grub

	# Prepare grub-efi bootloader
	mkdir -p "$DESTDIR/efi/boot"
	grub_gen_earlyconf > "$WORKDIR/grub_early.$3.cfg"
	grub-mkimage \
		--config="$WORKDIR/grub_early.$3.cfg" \
		--directory="$WORKDIR"/usr/lib/grub/"$_format" \
		--prefix="/boot/grub" \
		--output="$DESTDIR/efi/boot/$_efi" \
		--format="$_format" \
		--compression="xz" \
		$grub_mod
}

section_grubieee1275() {
	[ "$ARCH" = ppc64le ] || return 0
	[ "$output_format" = "iso" ] || return 0
	kernel_cmdline="$kernel_cmdline console=hvc0"

	build_section grub_cfg boot/grub/grub.cfg $(grub_gen_config | checksum)
}

section_grub_efi() {
	[ -n "$grub_mod" ] || return 0
	local _format _efi
	case "$ARCH" in
	aarch64)_format="arm64-efi";  _efi="bootaa64.efi" ;;
	arm*)	_format="arm-efi";    _efi="bootarm.efi"  ;;
	x86)	_format="i386-efi";   _efi="bootia32.efi" ;;
	x86_64) _format="x86_64-efi"; _efi="bootx64.efi"  ;;
	riscv64) _format="riscv64-efi"; _efi="bootriscv64.efi"  ;;
	loongarch64) _format="loongarch64-efi"; _efi="bootloongarch64.efi"  ;;
	*)	return 0 ;;
	esac

	build_section grub_cfg boot/grub/grub.cfg $(grub_gen_config | checksum)
	build_section grub_efi $_format $_efi $(grub_gen_earlyconf | checksum)
}

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
	if [ -e "${DESTDIR}/efi" -a -e "${DESTDIR}/boot/grub" ]; then
		# Create the EFI boot partition image
		mformat -i ${DESTDIR}/boot/grub/efi.img -C -f 1440 -N 0 ::
		mcopy -i ${DESTDIR}/boot/grub/efi.img -s ${DESTDIR}/efi ::
		touch -md "@${SOURCE_DATE_EPOCH}" ${DESTDIR}/boot/grub/efi.img

		# Enable EFI boot
		if [ -z "$_isolinux" ]; then
			# efi boot only
			_efiboot="
				-efi-boot-part
				--efi-boot-image
				-e boot/grub/efi.img
				-no-emul-boot
				"
		else
			# hybrid isolinux+efi boot
			_efiboot="
				-eltorito-alt-boot
				-e boot/grub/efi.img
				-no-emul-boot
				-isohybrid-gpt-basdat
				"
		fi
	fi

	if [ "$ARCH" = ppc64le ]; then
		apk fetch --root "$APKROOT" --quiet --stdout grub-ieee1275 | tar -C "$WORKDIR" -zx usr/lib/grub
		grub-mkrescue --output ${ISO} ${DESTDIR} -follow-links \
			--directory="$WORKDIR"/usr/lib/grub/powerpc-ieee1275 \
			-sysid LINUX \
			-volid "$(gen_volid)"
	else
		if [ "$ARCH" = s390x ]; then
			printf %s "$initfs_cmdline $kernel_cmdline " > ${WORKDIR}/parmfile
			for _f in $kernel_flavors; do
				mk-s390-cdboot -p ${WORKDIR}/parmfile \
					-i ${DESTDIR}/boot/vmlinuz-$_f \
					-r ${DESTDIR}/boot/initramfs-$_f \
					-o ${DESTDIR}/boot/merged.img
			done
			iso_opts="$iso_opts -no-emul-boot -eltorito-boot boot/merged.img"
		fi
		xorrisofs \
			-quiet \
			-output ${ISO} \
			-full-iso9660-filenames \
			-joliet \
			-rational-rock \
			-sysid LINUX \
			-volid "$(gen_volid)" \
			$_isolinux \
			$_efiboot \
			-follow-links \
			${iso_opts} \
			${DESTDIR}
	fi
}

create_image_targz() {
	tar -C "${DESTDIR}" \
		--mtime="@${SOURCE_DATE_EPOCH}" \
		--owner=0 --group=0 --numeric-owner \
		-chzf "${OUTDIR}/${output_filename}" .
}

profile_base() {
	kernel_flavors="lts"
	initfs_cmdline="modules=loop,squashfs,sd-mod,usb-storage quiet"
	initfs_features="ata base bootchart cdrom ext4 mmc nvme raid scsi squashfs usb virtio"
	modloop_sign=yes
	grub_mod="all_video disk part_gpt part_msdos linux normal configfile search search_label efi_gop fat iso9660 cat echo ls test true help gzio"
	case "$ARCH" in
		x86*) grub_mod="$grub_mod multiboot2 efi_uga";;
	esac
	case "$ARCH" in
		x86_64) initfs_features="$initfs_features nfit";;
		arm*|aarch64) initfs_features="$initfs_features phy";;
	esac
	apks="alpine-base apk-cron busybox chrony dhcpcd doas e2fsprogs
		kbd-bkeymaps network-extras openntpd openssl openssh
		tzdata wget tiny-cloud-alpine"
	apkovl=
	hostname="alpine"
}
