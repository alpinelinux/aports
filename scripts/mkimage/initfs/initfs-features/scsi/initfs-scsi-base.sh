initfs_scsi_base() {
	return 0
}

_initfs_scsi_base_modules() {
	cat <<-'EOF'
		kernel/drivers/scsi/scsi_mod.ko
		kernel/drivers/scsi/sd_mod.ko
		kernel/drivers/scsi/sr_mod.ko
		kernel/drivers/scsi/st.ko
		kernel/drivers/scsi/sg.ko
	EOF
}

