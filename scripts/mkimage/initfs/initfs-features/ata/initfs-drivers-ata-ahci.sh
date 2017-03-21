initfs_ata_drivers_ahci() {
	return 0
}

_initfs_ata_drivers_ahci_modules() {
	cat <<-'EOF'
		kernel/drivers/ata/libata*
		kernel/drivers/ata/*ahci*
	EOF
}

_initfs_ata_drivers_ahci_files() { return 0 ; }

