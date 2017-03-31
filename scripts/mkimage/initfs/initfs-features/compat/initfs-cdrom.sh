# Initifs feature cdrom.

initfs_cdrom() {
	return 0
}

_initfs_cdrom_modules() {
	cat <<-'EOF'
		kernel/drivers/cdrom
		kernel/fs/isofs
	EOF
}

_initfs_cdrom_files() {
	return 0
}
