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

_initfs_fs_ext2_files() { return 0 ; }



initfs_fs_ext3() {
	return 0
}

_initfs_fs_ext3_modules() {
	_initfs_fs_ext4_modules
}

_initfs_fs_ext3_files() { return 0 ; }



initfs_fs_ext4() {
	return 0
}

_initfs_fs_ext4_modules() {
	cat <<-EOF
		kernel/fs/ext4/
	EOF
}

_initfs_fs_ext4_files() { return 0 ; }
