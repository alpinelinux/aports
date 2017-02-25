# iSCSI initiator/target (client/server) features

feature_iscsi() {
	feature_iscsi_initiator
	feature_iscsi_target
}


# iSCSI initiator (client) feature
# iSCSI rootfs support in initfs enabled by default, use enable_iscsi_rootfs="false" to disable.
# TODO: Finish rootfs-on-iscsi support

feature_iscsi_initiator() {


	enable_rootfs_iscsi="${enable_rootfs_iscsi:=true}"

	if [ "$enable_rootfs_iscsi" = "true" ] ; then
		initfs_features="$initfs_features scsi"
		initfs_apks="$initfs_apks open-iscsi"
	else
		apks="$apks open-iscsi"
	fi
}


#iSCSI target (host) feature

feature_iscsi_target() {
	apks="$apks targetcli"
}

