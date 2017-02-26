# ZFS filesystem feature.
# Enables mkinitfs "zfs" feature by default use enable_rootfs_zfs="false" to disable.
feature_zfs() {

	enable_rootfs_zfs="${enable_rootfs_zfs:=true}"
	
	if [ "$enable_rootfs_zfs" = "true" ] ; then
		initfs_apks="$initfs_apks zfs"
		initfs_apks_flavored="$initfs_apks_flavored spl zfs"
		initfs_features="$initfs_features zfs"
	else
		apks_flavored="$apks_flavord spl zfs"
		apks="$apks zfs"
	fi

	apks="$apks udev zfs-udev"

	overlays="$overlays zfs"
}
