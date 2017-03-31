# Initifs feature mmc.

initfs_mmc() {
	return 0
}

_initfs_mmc_modules() {
	cat <<-'EOF'
		kernel/drivers/mmc
	EOF
}

_initfs_mmc_files() {
	return 0
}
