
initfs_drivers_cdrom() {
	return 0
}

_initfs_drivers_cdrom() {
	cat <<-'EOF'
		kernel/drivers/cdrom
		kernel/fs/isofs
	EOF
}

