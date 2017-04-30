initfs_feature_scsi_all() { return 0 ; }

_initfs_feature_scsi_all_modules() {
	cat <<-'EOF'
		kernel/drivers/scsi/
		kernel/drivers/message/fusion/
	EOF
}

