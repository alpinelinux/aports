# Initifs feature raid.

initfs_raid() {
	return 0
}

_initfs_raid_modules() {
	cat <<-'EOF'
		kernel/drivers/md/raid*
		kernel/drivers/block/cciss*
		kernel/drivers/block/sx8*
		kernel/arch/*/crypto/crc32*
		kernel/crypto/crc32*
	EOF
}

_initfs_raid_files() {
	cat <<-'EOF'
		/etc/mdadm.conf
		/sbin/mdadm
	EOF
}
