# Initifs feature ata.

initfs_ata() {
	return 0
}

_initfs_ata_modules() {
	cat <<-'EOF'
		kernel/drivers/ata/*.ko
	EOF
}

_initfs_ata_files() {
	return 0
}
