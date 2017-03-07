plugin_kernels() {
	var_list_alias initfs_apks
	var_list_alias initfs_only_apks
	var_list_alias initfs_features
	return 0
}

plugin_kernel_flavors() {
	var_list_alias kernel_flavors
	var_list_alias initfs_apks_flavored
	var_list_alias initfs_only_apks_flavored
	return 0
}

# Helpers to defined kernel-flavor suffixes to all specified roots.
suffix_kernel_flavor() {
	local _flavor="$1"
	shift

	list_add_suffix "-${_flavor}" $@
}

suffix_kernel_flavors() {
	local _flavor
	for _flavor in $kernel_flavors; do
		suffix_kernel_flavor $_flavor $@
	done
}

section_kernels() {
	local _flavor _pkgs
	for _flavor in $kernel_flavors; do
		#FIXME Should these really be hard-coded?
		var_list_set _pkgs "$(suffix_kernel_flavor "linux") linux-firmware"

		var_list_add _pkgs "$initfs_apks $initfs_only_apks"
		var_list_add _pkgs "$(suffix_kernel_flavor $_flavor $initfs_apks_flavored $initfs_only_apks_flavored)"

		local id=$( (echo "$initfs_features::$_hostkeys" ; $APK fetch --root "$APKROOT" --simulate alpine-base $_pkgs | sort -u ) | checksum)
		build_section kernel $ARCH $_flavor $id $_pkgs
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
