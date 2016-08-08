build_kernel() {
	local _flavor="$2"
	shift 3
	local _pkgs="$@"
	update-kernel \
		$_hostkeys \
		--media \
		--flavor "$_flavor" \
		--arch "$ARCH" \
		--package "$_pkgs" \
		--feature "$initfs_features" \
		--repositories-file "$APKROOT/etc/apk/repositories" \
		"$DESTDIR"
}

section_kernels() {
	local _f _a _pkgs
	for _f in $kernel_flavors; do
		_pkgs="linux-$_f linux-firmware"
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
	local _host="$1"
	msg "Generating $_host.apkovl.tar.gz"
	(local _pwd=$PWD; cd "$DESTDIR"; fakeroot "$_pwd"/"$apkovl" "$_host")
}

section_apkovl() {
	[ -n "$apkovl" -a -n "$hostname" ] || return 0
	build_section apkovl $hostname $(checksum < "$apkovl")
}

build_syslinux() {
	local _fn
	mkdir -p "$DESTDIR"/boot/syslinux
	apk fetch --root "$APKROOT" --stdout syslinux | tar -C "$DESTDIR" -xz usr/share/syslinux
	for _fn in isolinux.bin ldlinux.c32 libutil.c32 libcom32.c32 mboot.c32; do
		mv "$DESTDIR"/usr/share/syslinux/$_fn "$DESTDIR"/boot/syslinux/$_fn || return 1
	done
	rm -rf "$DESTDIR"/usr
}

section_syslinux() {
	[ "$output_format" = "iso" ] || return 0
	build_section syslinux $(apk fetch --root "$APKROOT" --simulate syslinux | sort | checksum)
}

syslinux_gen_config() {
	[ -z "$syslinux_serial" ] || echo "SERIAL $syslinux_serial"
	echo "TIMEOUT ${syslinux_timeout:-20}"
	echo "PROMPT ${syslinux_prompt:-1}"
	echo "DEFAULT ${kernel_flavors%% *}"

	local _f
	for _f in $kernel_flavors; do
		if [ -z "${xen_params+set}" ]; then
			cat <<EOF

LABEL $_f
	MENU LABEL Linux $_f
	KERNEL /boot/vmlinuz-$_f
	INITRD /boot/initramfs-$_f
	DEVICETREEDIR /boot/dtbs
	APPEND $initfs_cmdline $kernel_cmdline
EOF
		else
			cat <<EOF

LABEL $_f
	MENU LABEL Xen/Linux $_f
	KERNEL /boot/syslinux/mboot.c32
	APPEND /boot/xen.gz ${xen_params} --- /boot/vmlinuz-$_f $initfs_cmdline $kernel_cmdline --- /boot/initramfs-$_f
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
	[ ! "$output_format" = "iso" ] || syslinux_cfg="boot/syslinux/syslinux.cfg"
	[ ! -n "$uboot_install" ] || syslinux_cfg="extlinux/extlinux.conf"
	[ -n "$syslinux_cfg" ] || return 0
	build_section syslinux_cfg $syslinux_cfg $(syslinux_gen_config | checksum)
}

create_image_iso() {
	local ISO="${OUTDIR}/${output_filename}"
	xorrisofs \
		-o ${ISO} -l -J -R \
		-b boot/syslinux/isolinux.bin		\
		-c boot/syslinux/boot.cat		\
		-V "alpine-$PROFILE $RELEASE $ARCH"	\
		-no-emul-boot		\
		-boot-load-size 4	\
		-boot-info-table	\
		-quiet			\
		-follow-links		\
		${iso_opts}		\
		${DESTDIR}
	isohybrid ${ISO}
}

create_image_targz() {
	tar -C "${DESTDIR}" -chzf ${OUTDIR}/${output_filename} .
}

profile_base() {
	kernel_flavors="grsec"
	initfs_cmdline="modules=loop,squashfs,sd-mod,usb-storage quiet"
	initfs_features="ata base bootchart cdrom squashfs ext2 ext3 ext4 mmc raid scsi usb virtio"
	apks="alpine-base alpine-mirrors bkeymaps chrony e2fsprogs network-extras openssl openssh tzdata"
	apkovl="genapkovl-dhcp.sh"
	hostname="alpine"
}
