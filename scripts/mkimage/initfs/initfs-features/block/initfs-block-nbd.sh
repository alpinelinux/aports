initfs_block_nbd() {
	return 0
}

_initfs_block_nbd_modules() {
	cat <<-'EOF'
		kernel/drivers/block/nbd.ko
	EOF
}

_initfs_block_nbd_files() {
	cat <<-'EOF'
		/usr/sbin/nbd-client
	EOF
}

