# Build all specified kernels.
build_all_kernels() {
	local _flavor

	fkrt_faked_start kernel
	for _flavor in $(get_kernel_flavors); do
		build_kernel "$_flavor"
	done
	fkrt_faked_stop kernel
}

# Build a specific kerenel config.
build_kernel() {
	local _flavor="$1"

	fkrt_enable kernel

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
		mkdir -p "$KERNEL_STAGING"
		KERNEL_STAGING="$(realpath $KERNEL_STAGING)"


		build_section kernel_stage_begin "$KERNEL_STAGING" $ARCH $id_base
		build_section kernel_stage_packaged_kernel "$KERNEL_STAGING"  $ARCH $id_kern $_flavor $_kpkg $_kpkg_full $_kver_rel
		build_section kernel_stage_packaged_modules "$KERNEL_STAGING"  $ARCH $id_kern $_flavor $_kpkg $_kpkg_full $_kver_rel
		build_section kernel_stage_packaged_firmware "$KERNEL_STAGING"  $ARCH $id_firm $_fpkg $_fpkg_full
		build_section kernel_stage_packaged_dtbs "$KERNEL_STAGING"  $ARCH $id_kern $_flavor $_kpkg $_kpkg_full $_kver_rel
		build_section kernel_stage_added_pkgs "$KERNEL_STAGING" $ARCH $id_pkgs $_pkgs
		build_section kernel_stage_added_pkgs_flavored "$KERNEL_STAGING" $ARCH $id_pkgs_flavored $_flavor $_pkgs_flavored

		# Merge the individual staged directories:
		build_section kernel_stage_merge "$KERNEL_STAGING" $ARCH $id_stage $_kver_rel "base kernel modules firmware dtbs addpkgs addpkgs_flavored"

		# Build modloop, initramfs, devicetree
		# TODO: Modify this to allow selecting which modules are installed in modloop!
		build_section kernel_stage_modloop "$KERNEL_STAGING" $ARCH $id_modloop $_flavor $_kpkg $_kpkg_full $_kver_rel
		build_section kernel_stage_mkinitfs "$KERNEL_STAGING" $ARCH $id_mkinitfs $_flavor $_kver_rel $initfs_features
		build_section kernel_stage_devicetree "$KERNEL_STAGING" $ARCH $id_kern $_flavor $_kpkg $_kpkg_full $_kver_rel

		# Install the final output in $DESTDIR here.
		build_section kernel_install "$KERNEL_STAGING" $ARCH $id_install "kernel modloop mkinitfs devicetree"

	fkrt_disable
}

# Begin build by making staging directory
build_kernel_stage_begin() {
	rm -rf "$1"
	mkdir -p "$1/base"
	mkdir -p "$1-apks/alpine-baselayout"

	_apk fetch -R -o "$1-apks/alpine-baselayout" "alpine-baselayout"

	find "$1-apks/alpine-baselayout" -name '*.apk' -exec tar -xzp -C "$1/base" -f \{\} \;
	rm -r "$1/base"/.[_[:alnum:]]*

	mkdir -p "$1/base/etc/apk/keys"

	cp "$APKREPOS" "$1/base/etc/apk/"
	[ "$_abuild_pubkey" ] && cp -Pr "$1" "$1/base/etc/apk/keys/"
	[ "$_hostkeys" ] && cp -Pr /etc/apk/keys/* "$1/base/etc/apk/keys/"

	rm -rf "$DESTDIR"
	mkdir -p "$1/base/.built"
	ln -sfT "$1/base/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}

# Custom kernel build directories.
# TODO: Not yet implemented -- need to chase config options to enable
build_kernel_stage_custom_kernel() {
	mkdir -p "$1/kernel"
	make -C "$BUILDDIR" INSTALL_PATH="$1/kernel/boot" ${kernel_make_install_target:-install}
	rm -rf "$DESTDIR"
	mkdir -p "$1/kernel/.built"
	ln -sfT "$1/kernel/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}
build_kernel_stage_custom_modules() {
	mkdir -p "$1/modules"
	make -C "$BUILDDIR" INSTALL_MOD_PATH="$1/modules" modules_install
	rm -rf "$DESTDIR"
	mkdir -p "$1/modules/.built"
	ln -sfT "$1/modules/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}
build_kernel_stage_custom_firmware() {
	mkdir -p "$1/firmware"
	make -C "$BUILDDIR" INSTALL_MOD_PATH="$1/firmware" firmware_install
	rm -rf "$DESTDIR"
	mkdir -p "$1/firmware/.built"
	ln -sfT "$1/firmware/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}
build_kernel_stage_custom_dtbs() {
	mkdir -p "$1/dtbs"
	make -C "$BUILDDIR" INSTALL_DTBS_PATH="$1/dtbs/usr/lib/linux/linux-$7" dtbs_install
	rm -rf "$DESTDIR"
	mkdir -p "$1/dtbs/.built"
	ln -sfT "$1/dtbs/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}


# Using standard kernel packages
build_kernel_stage_packaged_kernel() {
	mkdir -p "$1/kernel"
	mkdir -p "$1-apks/kernel"
	_apk fetch -o "$1-apks/kernel" "$5"
	tar -xz -C "$1/kernel" -f "$1-apks/kernel/$(apk_pkg_full "$5").apk" boot || return 1
	rm -rf "$DESTDIR"
	mkdir -p "$1/kernel/.built"
	ln -sfT "$1/kernel/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}

build_kernel_stage_packaged_modules() {
	mkdir -p "$1/modules"
	mkdir -p "$1-apks/kernel"
	_apk fetch -o "$1-apks/kernel" "$5"
	tar -xz -C "$1/modules" -f "$1-apks/kernel/$(apk_pkg_full "$5").apk" lib/modules || return 1
	rm -rf "$DESTDIR"
	mkdir -p "$1/modules/.built"
	ln -sfT "$1/modules/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}

build_kernel_stage_packaged_firmware() {
	mkdir -p "$1/firmware"
	mkdir -p "$1-apks/firmware"
	_apk fetch -o "$1-apks/firmware" "$4"
	tar -xz -C "$1/firmware" -f "$1-apks/firmware/$(apk_pkg_full "$4").apk" lib/firmware || return 1
	rm -rf "$DESTDIR"
	mkdir -p "$1/firmware/.built"
	ln -sfT "$1/firmware/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}

build_kernel_stage_packaged_dtbs() {
	mkdir -p "$1/dtbs"
	mkdir -p "$1-apks/kernel"
	_apk fetch -o "$1-apks/kernel" "$5"
	if ( tar -tz -f "$1-apks/kernel/$(apk_pkg_full "$5").apk" | grep -q "^usr/lib" ) ; then
		tar -xz -C "$1/dtbs" -f "$1-apks/kernel/$(apk_pkg_full "$5").apk" usr/lib || return 1
	fi
	rm -rf "$DESTDIR"
	mkdir -p "$1/dtbs/.built"
	ln -sfT "$1/dtbs/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}

# Additional packages needed by mkinitfs for various feature files
build_kernel_stage_added_pkgs() {
	local base="$1"

	rm -rf "$base/addpkgs$__extra"
	mkdir -p "$base-apks/addpkgs$__extra"
	mkdir -p "$base/addpkgs$__extra"
	shift 3

	local p
	for p in $@ ; do
		_apk fetch -R -o "$base-apks/addpkgs$__extra" "$p"
	done

	local f
	local d="$base-apks/addpkgs$__extra"
	find "$d" -name '*.apk' -exec tar -xz -C "$base/addpkgs$__extra" -f \{\} \;
	rm -r "$base/addpkgs$__extra"/.[_[:alnum:]]*

	rm -rf "$DESTDIR"
	mkdir -p "$base/addpkgs$__extra/.built"
	ln -sfT "$base/addpkgs$__extra/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
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

	rm -rf
	mkdir -p "$base/merged"

	local d f
	for d in $@ ; do
		f="${base}/${d}"
		[ -d "$f" ] && ( cd "$f" && find | cpio -pumd "$base/merged" )
	done

	depmod -b "$base/merged" "$_kver"

	rm -rf "$DESTDIR"
	mkdir -p "$base/merged/.built"
	ln -sfT "$base/merged/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}


# Build our modloop
build_kernel_stage_modloop() {
	local base="$1"
	local _flavor="$4"

	local _out="$base/modloop"
	local _tmp="$_out-tmp"
	local _outname="modloop-$_flavor"

	rm -rf "$_tmp" "$_out"
	mkdir -p "$_out/boot" "$_tmp/lib"

	# TODO: Allow filtering of modloop modules here!
	cp -a "$base/merged/lib/modules" "$_tmp/modules"
	mkdir -p "$_tmp/modules/firmware"
	find "$_tmp/modules" -type f -name "*.ko" | xargs modinfo -F firmware | sort -u | while read FW; do
		if [ -e "$base/merged/lib/firmware/$FW" ]; then
			install -pD "$base/merged/lib/firmware/$FW" "$_tmp/modules/firmware/$FW"
		fi
	done
	mksquashfs "$_tmp" "$_out/boot/$_outname" -comp xz -exit-on-error

	rm -rf "$DESTDIR"
	mkdir -p "$_out/.built"
	ln -sfT "$_out/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}

# Build our initfs
build_kernel_stage_mkinitfs() {
	local base="$1"
	local _flavor="$4"
	local _kver="$5"
	shift 5

	local _out="$base/mkinitfs"
	local _tmp="$_out-tmp"

	rm -rf "$_out" "$_tmp"

	mkdir -p "$_out/boot" "$_tmp"
	mkdir -p "$base/merged/etc/mkinitfs"
	echo "features=\"$@\"" > "$base/merged/etc/mkinitfs/mkinitfs.conf"
	# mkinitfs keep-tmp install-keys base-dir features output tmp-dir kernel-version
	# TODO: mkinitfs to use should be configurable.
	msg "calling: mkinitfs -K -P /etc/mkinitfs/features.d -b $base/merged -c $base/merged/etc/mkinitfs/mkinitfs.conf -o $_out/boot/initramfs-$_flavor $_kver"
	mkinitfs -k -K -P /etc/mkinitfs/features.d -b "$base/merged" -c "$base/merged/etc/mkinitfs/mkinitfs.conf" -t "$_tmp" -o "$_out/boot/initramfs-$_flavor" "$_kver"

	rm -rf "$DESTDIR"
	mkdir -p "$_out/.built"
	ln -sfT "$_out/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}


# Build our device tree
build_kernel_stage_devicetree () {
	local base="$1"
	local _kver="$7"
	local _in="$base/merged/usr/lib/linux-$_kver"
	local _out="$base/devicetree/boot/dtbs"

	rm -rf "$base/devicetree"
	mkdir -p "$base/devicetree"

	if [ -d "$_in" ]; then
		mkdir -p "$_out"
		_out=$(realpath "$_out")
		(cd $_in && find -type f \( -name "*.dtb" -o -name "*.dtbo" \) | cpio -pudm "$_out" 2> /dev/null)
	fi

	rm -rf "$DESTDIR"
	mkdir -p "$base/devicetree/.built"
	ln -sfT "$base/devicetree/.built" "$DESTDIR"
	#mkdir -p "$DESTDIR"
}

# Install everything in our DESTDIR ready for merging into the final image
build_kernel_install() {
	local base="$1"
	shift 3

	local _out="$DESTDIR"
	rm -rf "$_out"
	mkdir -p "$_out"

	# TODO: Handle differnt output directories (rpi?) for various bits when needed.
	local d f
	for d in $@ ; do
		f="$base/$d"
		[ -d "$f" ] && ( cd "$f" && find | grep -v '^\./\.built' | cpio -pumd "$_out" )
	done

	# TODO: Review "media" logic and implement if needed.
	if [ "$media" = "false" ] ; then
		mv "$_out/boot/"* "$_out"
		rmdir "$_out/boot"
	fi
}
