
initfs_drivers_cdrom() {
	return 0
}

_initfs_drivers_cdrom_modules() {
	cat <<-'EOF'
		kernel/drivers/cdrom
		kernel/fs/isofs
	EOF
}

_initfs_drivers_cdrom_files() { return 0 ; }
