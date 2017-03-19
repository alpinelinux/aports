# Build all specified kernels.
build_all_kernels() {
	local _flavor _err

	fkrt_faked_start kernel
	for _flavor in $(get_kernel_flavors); do
		_err=1
		fkrt_enable_kernel
		build_kernel "$_flavor" && _err=0 || break
		fkrt_disable
	done
	fkrt_faked_stop kernel

	return $_err
}

# Build a specific kerenel config.
build_kernel() {
	local _flavor="$1"

	local _pkgs

	#FIXME Should these really be hard-coded?
	local _kpkg=$(suffix_kernel_flavor $_flavor "linux") && _kpkg=${_kpkg// /}
	local _fpkg=linux-firmware

	local _kpkg_full=$( printf %s $(apk_pkg_full $_kpkg)) && _kpkg_full=${_kpkg_full// /}
	local _fpkg_full=$(apk_pkg_full $_fpkg)

	local _kver_full=${_kpkg_full#$_kpkg-}
	local _kver_short=${_kver_full%-r*}
	local _kver_rev=${_kver_full#$_kver_short-r}
	local _kver_rel=$_kver_short-$_kver_rev-$_flavor

	var_list_set _pkgs_flavored "$(suffix_kernel_flavor $_flavor $initfs_apks_flavored $initfs_only_apks_flavored)"
	var_list_set _pkgs "$initfs_apks $initfs_only_apks"

	local id_base=$(_apk fetch -R --simulate alpine-baselayout | sort -u | checksum)
	local id_kern=$(_apk fetch -R --simulate $_kpkg | sort -u  | checksum)
	local id_firm=$(_apk fetch -R --simulate $_fpkg | sort -u  | checksum)
	local id_pkgs=$(_apk fetch -R --simulate alpine-baselayout $_pkgs | sort -u  | checksum)
	local id_pkgs_flavored=$(_apk fetch -R --simulate $_pkgs_flavored | sort -u  | checksum)
	local id_stage=$(printf '%s' $id_base $id_kern $id_firm $id_pkgs $id_pkgs_flavored| checksum)
	local id_modloop=$(printf '%s' $id_kern $id_firm $id_pkgs_flavored | checksum)
	local id_mkinitfs=$(printf '%s' $id_kern $id_firm $id_pkgs $id_pkgs_flavored $id_stage $initfs_features | checksum)
	local id_install=$(printf '%s' $id_kern $id_firm $id_pkgs $id_pkgs_flavored $id_stage $id_modloop $id_mkinitfs | checksum)

	# Stage everything so we don't have to repeat ourselves!
	KERNEL_STAGING="kernel_staging-$ARCH-$_kpkg_full-$id_kern"
	KERNEL_STAGING="${KERNEL_STAGING//[^A-Za-z0-9]/_}"
	KERNEL_STAGING="${WORKDIR}/${KERNEL_STAGING}"
	mkdir_is_writable "$KERNEL_STAGING"
	KERNEL_STAGING="$(realpath $KERNEL_STAGING)"


	build_section kernel_stage_begin "$KERNEL_STAGING" $ARCH $id_base || return 1
	build_section kernel_stage_packaged_kernel "$KERNEL_STAGING"  $ARCH $id_kern $_flavor $_kpkg $_kpkg_full $_kver_rel || return 1
	build_section kernel_stage_packaged_modules "$KERNEL_STAGING"  $ARCH $id_kern $_flavor $_kpkg $_kpkg_full $_kver_rel || return 1
	build_section kernel_stage_packaged_firmware "$KERNEL_STAGING"  $ARCH $id_firm $_fpkg $_fpkg_full || return 1
	build_section kernel_stage_packaged_dtbs "$KERNEL_STAGING"  $ARCH $id_kern $_flavor $_kpkg $_kpkg_full $_kver_rel || return 1
	build_section kernel_stage_added_pkgs "$KERNEL_STAGING" $ARCH $id_pkgs $_pkgs || return 1
	build_section kernel_stage_added_pkgs_flavored "$KERNEL_STAGING" $ARCH $id_pkgs_flavored $_flavor $_pkgs_flavored || return 1

	# Merge the individual staged directories:
	build_section kernel_stage_merge "$KERNEL_STAGING" $ARCH $id_stage $_kver_rel "base kernel modules firmware dtbs addpkgs addpkgs_flavored" || return 1

	# Build modloop, initramfs, devicetree
	# TODO: Modify this to allow selecting which modules are installed in modloop!
	build_section kernel_stage_modloop "$KERNEL_STAGING" $ARCH $id_modloop $_flavor $_kpkg $_kpkg_full $_kver_rel || return 1
	build_section kernel_stage_mkinitfs "$KERNEL_STAGING" $ARCH $id_mkinitfs $_flavor $_kver_rel $initfs_features || return 1
	build_section kernel_stage_devicetree "$KERNEL_STAGING" $ARCH $id_kern $_flavor $_kpkg $_kpkg_full $_kver_rel || return 1

	# Install the final output in $DESTDIR here.
	build_section kernel_install "$KERNEL_STAGING" $ARCH $id_install "kernel modloop mkinitfs devicetree" || return 1
}

# Begin build by making staging directory
build_kernel_stage_begin() {
	rm -rf "$1"
	mkdir_is_writable "$1/base" || ! warning "Can not create writable directory: '$1/base'" || return 1
	mkdir_is_writable "$1-apks/alpine-baselayout"

	_apk fetch -R -o "$1-apks/alpine-baselayout" "alpine-baselayout"

	find "$1-apks/alpine-baselayout" -name '*.apk' -exec tar -xzp -C "$1/base" -f \{\} \;
	rm -r "$1/base"/.[!.]*

	mkdir_is_writable "$1/base/etc/apk/keys"

	cp "$APKREPOS" "$1/base/etc/apk/"
	[ "$_abuild_pubkey" ] && cp -Pr "$1" "$1/base/etc/apk/keys/"
	[ "$_hostkeys" ] && cp -Pr /etc/apk/keys/* "$1/base/etc/apk/keys/"

	rm -rf "$DESTDIR"
	mkdir_is_writable "$1/base/.built"
	ln -sfT "$1/base/.built" "$DESTDIR"
}

# Custom kernel build directories.
# TODO: Not yet implemented -- need to chase config options to enable
build_kernel_stage_custom_kernel() {
	mkdir_is_writable "$1/kernel"
	make -C "$BUILDDIR" INSTALL_PATH="$1/kernel/boot" ${kernel_make_install_target:-install}
	rm -rf "$DESTDIR"
	mkdir_is_writable "$1/kernel/.built"
	ln -sfT "$1/kernel/.built" "$DESTDIR"
}
build_kernel_stage_custom_modules() {
	mkdir_is_writable "$1/modules"
	make -C "$BUILDDIR" INSTALL_MOD_PATH="$1/modules" modules_install
	rm -rf "$DESTDIR"
	mkdir_is_writable "$1/modules/.built"
	ln -sfT "$1/modules/.built" "$DESTDIR"
}
build_kernel_stage_custom_firmware() {
	mkdir_is_writable "$1/firmware"
	make -C "$BUILDDIR" INSTALL_MOD_PATH="$1/firmware" firmware_install
	rm -rf "$DESTDIR"
	mkdir_is_writable "$1/firmware/.built"
	ln -sfT "$1/firmware/.built" "$DESTDIR"
}
build_kernel_stage_custom_dtbs() {
	mkdir_is_writable "$1/dtbs"
	make -C "$BUILDDIR" INSTALL_DTBS_PATH="$1/dtbs/usr/lib/linux/linux-$7" dtbs_install
	rm -rf "$DESTDIR"
	mkdir_is_writable "$1/dtbs/.built"
	ln -sfT "$1/dtbs/.built" "$DESTDIR"
}


# Using standard kernel packages
build_kernel_stage_packaged_kernel() {
	mkdir_is_writable "$1/kernel"
	mkdir_is_writable "$1-apks/kernel"
	_apk fetch -o "$1-apks/kernel" "$5"
	tar -xz -C "$1/kernel" -f "$1-apks/kernel/$(apk_pkg_full "$5").apk" boot \
			|| ! warning "Failed to extract kernel files from kernel apk: '$1-apks/kernel/$(apk_pkg_full "$5").apk'" \
			|| return 1
	rm -rf "$DESTDIR"
	mkdir_is_writable "$1/kernel/.built"
	ln -sfT "$1/kernel/.built" "$DESTDIR"
}

build_kernel_stage_packaged_modules() {
	mkdir_is_writable "$1/modules"
	mkdir_is_writable "$1-apks/kernel"
	_apk fetch -o "$1-apks/kernel" "$5"
	tar -xz -C "$1/modules" -f "$1-apks/kernel/$(apk_pkg_full "$5").apk" lib/modules \
			|| ! warning "Failed to extract modules from kernel apk: '$1-apks/kernel/$(apk_pkg_full "$5").apk'" \
			|| return 1
	rm -rf "$DESTDIR"
	mkdir_is_writable "$1/modules/.built"
	ln -sfT "$1/modules/.built" "$DESTDIR"
}

build_kernel_stage_packaged_firmware() {
	mkdir_is_writable "$1/firmware"
	mkdir_is_writable "$1-apks/firmware"
	_apk fetch -o "$1-apks/firmware" "$4"

	tar -xz -C "$1/firmware" -f "$1-apks/firmware/$(apk_pkg_full "$4").apk" lib/firmware \
			|| ! warning "Failed to extract firmware from firmware apk: '$1-apks/firmware/$(apk_pkg_full "$4").apk'" \
			|| return 1

	rm -rf "$DESTDIR"
	mkdir_is_writable "$1/firmware/.built"
	ln -sfT "$1/firmware/.built" "$DESTDIR"
}

build_kernel_stage_packaged_dtbs() {
	mkdir_is_writable "$1/dtbs"
	mkdir_is_writable "$1-apks/kernel"
	_apk fetch -o "$1-apks/kernel" "$5"

	if ( tar -tz -f "$1-apks/kernel/$(apk_pkg_full "$5").apk" | grep -q "^usr/lib" ) ; then
		tar -xz -C "$1/dtbs" -f "$1-apks/kernel/$(apk_pkg_full "$5").apk" usr/lib \
			|| ! warning "Failed to extract dtbs from kernel apk: '$1-apks/kernel/$(apk_pkg_full "$5").apk'" \
			|| return 1
	fi

	rm -rf "$DESTDIR"
	mkdir_is_writable "$1/dtbs/.built"
	ln -sfT "$1/dtbs/.built" "$DESTDIR"
}

# Additional packages needed by mkinitfs for various feature files
build_kernel_stage_added_pkgs() {
	local base="$1"
	shift 3

	local _out="$base/addpkgs$__extra"
	rm -rf "$_out" && mkdir_is_writable "$_out" && _out=$(realpath "$_out") || return 1

	local _outapks= "$base-apks/addpkgs$__extra"
	mkdir_is_writable "$_outapks" && _out=$(realpath "$_outapks") || return 1

	local p
	for p in $@ ; do
		_apk fetch -R -o "$_outapks" "$p"
	done

	if dir_is_readable "$_outapks" ; then
		find "$d" -name '*.apk' -exec tar -xz -C "$_out" -f \{\} \;
		ls -dA "$_out"/.[!.]* 2>&1 > /dev/null  && rm -rf "$_out"/.[!.]*
	fi

	rm -rf "$DESTDIR"
	mkdir_is_writable "$_out/.built"
	ln -sfT "$_out/.built" "$DESTDIR"
}
# Wrapper to swallow flavored packages
build_kernel_stage_added_pkgs_flavored() {
	local base="$1"
	local _arch="$2"
	local _id="$3"
	shift 4


	__extra="_flavored"
	build_kernel_stage_added_pkgs "$base" "$_arch" "$_id" "$@"
	unset __extra

}

# Merge everything we've extracted thus far into one directory
build_kernel_stage_merge() {
	local base="$1"
	local _kver="$4"
	shift 4

	local _out="$base/merged"
	rm -rf "$_out" && mkdir_is_writable "$_out" && _out=$(realpath "$_out") || return 1

	local d f
	for d in $@ ; do
		f="${base}/${d}"
		dir_is_readable "$f" && ( cd "$f" && find | cpio -pumd "$_out" )
	done

	depmod -b "$_out" "$_kver"

	rm -rf "$DESTDIR"
	mkdir_is_writable "$_out/.built"
	ln -sfT "$_out/.built" "$DESTDIR"
}


# Build our modloop
build_kernel_stage_modloop() {
	local base="$1"
	local _flavor="$4"

	dir_is_readable "$base/merged/lib/modules" || ! warning "Could not read merged modules directory '$base/merged/lib/modules'" || return 1
	dir_is_readable "$base/merged/lib/firmware" || ! warning "Could not read merged firmware directory '$base/merged/lib/firmware'" || return 1

	local _out="$base/modloop"
	rm -rf "$_out" && mkdir_is_writable "$_out" && _out=$(realpath "$_out") || return 1

	local _tmp="$_out-tmp"
	rm -rf "$_tmp"

	local _outname="modloop-$_flavor"

	mkdir_is_writable "$_out/boot"
	mkdir_is_writable "$_tmp/lib"

	# TODO: Allow filtering of modloop modules here!
	cp -a "$base/merged/lib/modules" "$_tmp/modules"
	mkdir -p "$_tmp/modules/firmware"
	find "$_tmp/modules" -type f -name "*.ko" | xargs modinfo -F firmware | sort -u | while read FW; do
		file_is_readable "$base/merged/lib/firmware/$FW" && install -pD "$base/merged/lib/firmware/$FW" "$_tmp/modules/firmware/$FW"
	done
	mksquashfs "$_tmp" "$_out/boot/$_outname" -comp xz -exit-on-error

	rm -rf "$DESTDIR"
	mkdir_is_writable "$_out/.built"
	ln -sfT "$_out/.built" "$DESTDIR"
}

# Build our initfs
build_kernel_stage_mkinitfs() {
	local base="$1"
	local _flavor="$4"
	local _kver="$5"
	shift 5

	dir_is_readable "$base/merged" || ! warning "Could not read merged directory root '$base/merged'" || return 1

	local _out="$base/mkinitfs"
	rm -rf "$_out" && mkdir_is_writable "$_out" && _out=$(realpath "$_out") || return 1

	local _tmp="$_out-tmp"
	rm -rf "$_tmp"

	mkdir_is_writable "$_out/boot"
	mkdir_is_writable "$_tmp"
	mkdir_is_writable "$base/merged/etc/mkinitfs"

	echo "features=\"$@\"" > "$base/merged/etc/mkinitfs/mkinitfs.conf"
	# mkinitfs keep-tmp install-keys feature-dir base-dir features-file tmp-dir output-file kernel-version
	# TODO: mkinitfs to use should be configurable.
	msg "calling: mkinitfs -K -P /etc/mkinitfs/features.d -b $base/merged -c $base/merged/etc/mkinitfs/mkinitfs.conf -t "$_tmp" -o $_out/boot/initramfs-$_flavor $_kver"
	mkinitfs -k -K -P /etc/mkinitfs/features.d -b "$base/merged" -c "$base/merged/etc/mkinitfs/mkinitfs.conf" -t "$_tmp" -o "$_out/boot/initramfs-$_flavor" "$_kver"

	rm -rf "$DESTDIR"
	mkdir_is_writable "$_out/.built"
	ln -sfT "$_out/.built" "$DESTDIR"
}


# Build our device tree
build_kernel_stage_devicetree () {
	local base="$1"
	local _kver="$7"
	local _in="$base/merged/usr/lib/linux-$_kver"
	local _out="$base/devicetree/boot/dtbs"

	rm -rf "$base/devicetree"
	mkdir_is_writable "$base/devicetree"

	if dir_is_readable "$_in" ; then
		rm -rf "$out" && mkdir_is_writable "$_out" && _out=$(realpath "$_out") || return 1
		(cd $_in && find -type f \( -name "*.dtb" -o -name "*.dtbo" \) | cpio -pudm "$_out" 2> /dev/null) || return 1
	fi

	rm -rf "$DESTDIR"
	mkdir_is_writable "$base/devicetree/.built"
	ln -sfT "$base/devicetree/.built" "$DESTDIR"
}

# Install everything in our DESTDIR ready for merging into the final image
build_kernel_install() {
	local base="$1"
	shift 3

	local _out="$DESTDIR"
	rm -rf "$_out" && mkdir_is_writable "$_out" && _out=$(realpath "$_out") || return 1

	# TODO: Handle differnt output directories (rpi?) for various bits when needed.
	local d f
	for d in $@ ; do
		f="$base/$d"
		dir_is_readable "$f" && ( cd "$f" && find | grep -v '^\./\.built' | cpio -pumd "$_out" ) || return 1
	done

	# TODO: Review "media" logic and implement if needed.
	if [ "$media" = "false" ] ; then
		mv "$_out/boot/"* "$_out"
		rmdir "$_out/boot"
	fi
}

