plugin_profiles() {
	var_list_alias apks
	var_list_alias apks_flavored
	var_list_alias arch
	var_list_alias initfs_apks
	var_list_alias initfs_apks_flavored
	var_list_alias initfs_only_apks
	var_list_alias initfs_only_apks_flavored
	var_list_alias initfs_features
	var_list_alias kernel_flavors
}


# Helpers to defined kernel-flavor suffixes to all specified roots.
suffix_kernel_flavor() {
	local _f="$1"
	shift
	for _a in $@; do
		_s="$s $_a-$_f"
	done
	echo $_s
}

suffix_all_kernel_flavors() {
	local _f _a _s=""
	for _f in $kernel_flavors; do
		suffix_kernel_flavor $_f $@
	done
	echo $_s
}

section_kernels() {
	local _f _a _pkgs
	for _f in $kernel_flavors; do
		#FIXME Should these really be hard-coded?
		var_list_set _pkgs "linux-$_f linux-firmware"

		var_list_add _pkgs "$initfs_apks $initfs_only_apks"
		var_list_add _pkgs "$(suffix_kernel_flavor $_f $initfs_apks_flavored $initfs_only_apks_flavored)"

		local id=$( (echo "$initfs_features::$_hostkeys" ; apk fetch --root "$APKROOT" --simulate alpine-base $_pkgs | sort) | checksum)
		build_section kernel $ARCH $_f $id $_pkgs
	done
}

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

# Local apk repository support.
section_apks() {
	# Build list of all required apks
	add_apks "$(suffix_all_kernel_flavors $apks_flavored) $initfs_apks $(suffix_all_kernel_flavors $initfs_apks_flavored)"
	[ -n "$apks" ] || return 0

	build_section apks $ARCH $(apk fetch --root "$APKROOT" --simulate --recursive $apks | sort | checksum)
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

