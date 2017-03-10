plugin_apks() {
	APK="${APK:-/usr/bin/abuild-apk}"
	var_list_alias apks
	var_list_alias apks_flavored
	var_list_alias host_apks
	var_list_alias rootfs_apks
	var_list_alias rootfs_apks_flavored
}

_apk() {
	local _apkcmd="$1"
	shift
	$APK $_apkcmd${APK_CACHE_DIR:+ --cache-dir "$APK_CACHE_DIR"}${APKROOT:+ --root "$APKROOT"}${ARCH:+ --arch "$ARCH"} "$@"
}

apk_repo_init() {
	local _work="$(realpath "$1")"
	local _arch="$2"
	info_func_set "apk-repo init"
	APKROOT="$_work/apkroot-$_arch"
	APKREPOS="$APKROOT/etc/apk/repositories"
	msg "Initilizing apk repository for '$_arch' at"
	msg2 "'$APKROOT'"
	if [ ! -e "$APKROOT" ]; then
		# create root for caching packages
		# TODO: Add configuration for source location of host keys
		cp -Pr /etc/apk/keys "$APKROOT/etc/apk/"
		_apk add --initdb

		if [ -z "$REPODIR" ] && [ -z "$REPOFILE" ]; then
			warning "no repository set"
		fi

		touch "$APKREPOS"

		for repo in $REPOFILE ; do
			cat "$REPOFILE" | grep -E -v "^#" >> "$APKREPOS"
		done

		[ -z "$REPODIR"] || echo "$REPODIR" >> "$APKREPOS"

		for repo in $EXTRAREPOS; do
			echo "$repo" >> "$APKREPOS"
		done
		msg "apk repository root for '$_arch' initilized."
	else
		msg "apk repository root for '$_arch' already initilized."
	fi
	_apk update
}

# Local apk repository support.
section_apks() {
	[ "disable_apk_repository" = "true" ] && return 0
	# Build list of all required apks

	# Check for any apks that need to go in the local repository.
	[ "${apks}${apks_flavored}${initfs_apks}${initfs_apks_flavored}${rootfs_apks}${rootfs_apks_flavored}" ] || return 0

	build_section apks $ARCH $(_apk fetch --simulate --recursive $apks | sort | checksum)
}

build_apks() {
	local _apksdir="$DESTDIR/apks"
	local _archdir="$_apksdir/$ARCH"
	info_func_set "build apk repo:$ARCH"

	msg "Building apk boot repository for '$ARCH':"
	mkdir -p "$_archdir"

	local _repoapks
	var_list_add _repoapks "$apks" "$(suffix_kernel_flavors $apks_flavored)"
	var_list_add _repoapks "$initfs_apks" "$(suffix_kernel_flavors $initfs_apks_flavored)"
	var_list_add _repoapks "$rootfs_apks" "$(suffix_kernel_flavors $rootfs_apks_flavored)"

	msg "Fetching required apks recursively..."
	msg2 "$_repoapks"
	_apk fetch --link --recursive --output "$_archdir" $_repoapks
	if ! ls "$_archdir"/*.apk >& /dev/null; then
		return 1
	fi
	msg "Creating APKINDEX..."
	_apk index \
		--description "$RELEASE" \
		--rewrite-arch "$ARCH" \
		--index "$_archdir"/APKINDEX.tar.gz \
		--output "$_archdir"/APKINDEX.tar.gz \
		"$_archdir"/*.apk

	msg "Signing APKINDEX..."
	abuild-sign "$_archdir"/APKINDEX.tar.gz
	touch "$_apksdir/.boot_repository"
	msg "APK boot repository built at '$_archdir'."
}

