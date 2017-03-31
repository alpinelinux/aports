# Initifs feature nvme.

initfs_nvme() {
	return 0
}

_initfs_nvme_modules() {
	cat <<-'EOF'
		kernel/drivers/nvme
	EOF
}

_initfs_nvme_files() {
	return 0
}
