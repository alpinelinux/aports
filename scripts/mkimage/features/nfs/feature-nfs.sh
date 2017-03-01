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
		: ##FIXME add_initfs_features "nfs"
	fi

	add_apks "nfs-utils"

}


# NFS server support

feature_nfs_server() {
	add_apks "nfs-utils"

}
