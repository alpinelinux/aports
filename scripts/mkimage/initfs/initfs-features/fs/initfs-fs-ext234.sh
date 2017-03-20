initfs_fs_ext_all() {
	initfs_fs_ext2
	initfs_fs_ext3
	initfs_fs_ext4
}

initfs_fs_ext2() {
	return 0
}

_initfs_fs_ext2_modules() {
	cat <<-EOF
		kernel/fs/ext2/
	EOF
}


initfs_fs_ext3() {
	return 0
}

_initfs_fs_ext3_modules() {
	_initfs_fs_ext4_modules
}


initfs_fs_ext4() {
	return 0
}

_initfs_fs_ext4_modules() {
	cat <<-EOF
		kernel/fs/ext4/
	EOF
}
