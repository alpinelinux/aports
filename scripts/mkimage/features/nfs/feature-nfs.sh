# Feature to include both client and server functionality for NFS.

feature_nfs() {
	feature_nfs_client
	feature_nfs_server
}


# NFS client support
# Use enable_rootfs_nfs="true" (default) to allow mounting the root filesystem over NFS

feature_nfs_client() {

	enable_rootfs_nfs="${enable_rootfs_nfs:=true}"
	if [ "$enable_rootfs_nfs" = "true" ] ; then
		add_initfs_features "fs-nfs"
	fi

	add_apks "nfs-utils"

}


# NFS server support

feature_nfs_server() {
	add_apks "nfs-utils"

}
