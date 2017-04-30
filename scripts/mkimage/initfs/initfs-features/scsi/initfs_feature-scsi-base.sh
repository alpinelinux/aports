initfs_feature_scsi_base() { return 0 ; }

_initfs_feature_scsi_base_modules() {
	cat <<-'EOF'
		scsi_mod
		sd_mod
		sr_mod
		st
		sg
	EOF
}

