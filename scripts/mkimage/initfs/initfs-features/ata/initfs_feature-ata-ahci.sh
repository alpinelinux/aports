initfs_feature_ata_ahci() { return 0 ; }

_initfs_feature_ata_ahci_modules() {
	cat <<-'EOF'
		kernel/drivers/ata/libata*
		kernel/drivers/ata/libahci*
		kernel/drivers/ata/ahci.ko
	EOF
}

