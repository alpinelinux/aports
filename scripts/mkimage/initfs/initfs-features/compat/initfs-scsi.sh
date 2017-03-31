# Initifs feature scsi.

initfs_scsi() {
	return 0
}

_initfs_scsi_modules() {
	cat <<-'EOF'
		kernel/drivers/scsi/*
		kernel/drivers/message/fusion
	EOF
}

_initfs_scsi_files() {
	return 0
}
