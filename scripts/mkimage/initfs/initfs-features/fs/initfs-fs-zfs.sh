initfs_fs_zfs() {
	return 0
}

_initfs_fs_zfs_modules() {
	cat <<-'EOF'
	extra/avl/
	extra/nvpair/
	extra/spl/
	extra/unicode/
	extra/zcommon/
	extra/zfs/
	extra/zpios/
	EOF
}

_initfs_fs_zfs_files() {
	cat <<-'EOF'
	/usr/sbin/zfs
	/usr/sbin/zpool
	EOF
}

