initfs_drivers_mmc() {
	return 0
}

_initfs_drivers_mmc_modules() {
	cat <<-'EOF'
		kernel/drivers/mmc/
	EOF
}

_initfs_drivers_mmc_files() { return 0 ; }
