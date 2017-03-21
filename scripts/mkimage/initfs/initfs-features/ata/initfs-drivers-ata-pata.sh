initfs_ata_drivers_pata() {
	return 0
}

_initfs_ata_drivers_pata_modules() {
	cat <<-'EOF'
		kernel/drivers/ata/libata.ko
		kernel/drivers/ata/pata*
		kernel/drivers/ata/ata*
		kernel/drivers/ata/pdc_*
	EOF
}

_initfs_ata_drivers_pata_files() { return 0 ; }

