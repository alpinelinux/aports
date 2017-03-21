initfs_ata_drivers_sata() {
	return 0
}

_initfs_ata_drivers_sata_modules() {
	cat <<-'EOF'
		kernel/drivers/ata/libata.ko
		kernel/drivers/ata/sata*
		kernel/drivers/ata/ata*
	EOF
}

_initfs_ata_drivers_sata_files() { return 0 ; }


