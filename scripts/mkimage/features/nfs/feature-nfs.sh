# Feature to include both client and server functionality for NFS.

feature_nfs() {
	feature_nfs_client
	feature_nfs_server
}


# NFS client support
# Use enable_rootfs_nfs="true" (default) to allow mounting the root filesystem over NFS
# FIXME: Add NFS feature to mkinitfs

feature_nfs_client() {

	enable_rootfs_nfs="${enable_rootfs_nfs:=true}"
	if [ "$enable_rootfs_nfs" = "true" ] ; then
##FIXME		initfs_features="$initfs_features nfs"
		initfs_features="$initfs_features"
	fi

	apks="$apks nfs-utils"

}


# NFS server support

feature_nfs_server() {
	apks="$apks nfs-utils"

}
