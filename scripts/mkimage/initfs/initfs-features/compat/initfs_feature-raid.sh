# Initifs feature raid.

initfs_feature_raid() { return 0 ; }

_initfs_feature_raid_modules() {
	cat <<-'EOF'
		kernel/drivers/md/raid*
		kernel/drivers/block/cciss*
		kernel/drivers/block/sx8*
		kernel/arch/*/crypto/crc32*
		kernel/crypto/crc32*
	EOF
}

_initfs_feature_raid_pkgs() {
	cat <<-'EOF'
		mdadm
	EOF
}

_initfs_feature_raid_files() {
	cat <<-'EOF'
		/etc/mdadm.conf
		/sbin/mdadm
	EOF
}
