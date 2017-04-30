# Initifs feature nvme.

initfs_feature_nvme() { return 0 ; }

_initfs_feature_nvme_modules() {
	cat <<-'EOF'
		kernel/drivers/nvme/
	EOF
}

