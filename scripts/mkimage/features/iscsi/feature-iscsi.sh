# iSCSI initiator/target (client/server) features

feature_iscsi() {
	feature_iscsi_initiator
	feature_iscsi_target
}


# iSCSI initiator (client) feature
# iSCSI rootfs support in initfs enabled by default, use enable_iscsi_rootfs="false" to disable.
# TODO: feature_iscsi - Finish rootfs-on-iscsi support

feature_iscsi_initiator() {


	enable_rootfs_iscsi="${enable_rootfs_iscsi:=true}"

	if [ "$enable_rootfs_iscsi" = "true" ] ; then
		add_initfs_features "scsi-iscsi"
		add_initfs_apks "open-iscsi"
	else
		add_apks "open-iscsi"
	fi
}


#iSCSI target (host) feature

feature_iscsi_target() {
	add_apks "targetcli"
}

