# Initifs feature scsi.

initfs_feature_scsi() { return 0 ; }

_initfs_feature_scsi_modules() {
	cat <<-'EOF'
		kernel/drivers/scsi/*
		kernel/drivers/message/fusion
	EOF
}

