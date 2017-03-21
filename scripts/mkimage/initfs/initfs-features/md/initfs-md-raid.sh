initfs_md_raid() {
	return 0
}

_initfs_md_raid_modules() {
	cat <<-'EOF'
		kernel/drivers/md/raid*
	EOF

	_initfs_crypto_crc32_modules
}

_initfs_md_raid_files() {
	cat <<-'EOF'
		/etc/mdadm.conf
		/sbin/mdadm
	EOF
}
