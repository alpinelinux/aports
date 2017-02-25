
feature_zfs() {
	initfs_apks_flavored="$initfs_apks_flavored spl zfs"
	initfs_apks="$initfs_apks zfs"
	initfs_features="$initfs_features zfs"
	apks_flavored="$apks_flavored $initfs_apks_flavored"
	apks="$apks zfs-udev"
}
