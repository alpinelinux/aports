initfs_ata_all() {
	return 0
}

_initfs_ata_all_modules() {
	cat <<-'EOF'
		kernel/drivers/ata/
	EOF
}

_initfs_ata_all_files() { return 0 ; }

