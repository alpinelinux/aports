initfs_dm_crypsetup() {
	return 0
}

initfs_dm_crypsetup() {
	cat <<-'EOF'
		kernel/drivers/md/dm-crypt.ko
	EOF
	_initfs_crypto_all
}

_initfs_dm_crypsetup_files() {
	cat <<-'EOF'
		/sbin/cryptsetup
	EOF
}

