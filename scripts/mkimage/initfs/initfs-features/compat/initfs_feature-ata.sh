# Initifs feature ata.

initfs_feature_ata() { return 0 ; }

_initfs_feature_ata_modules() {
	cat <<-'EOF'
		kernel/drivers/ata/*.ko
	EOF
}

