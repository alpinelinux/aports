initfs_feature_ata_all() { return 0 ;  }

_initfs_feature_ata_all_modules() {
	cat <<-'EOF'
		kernel/drivers/ata/
	EOF
}

