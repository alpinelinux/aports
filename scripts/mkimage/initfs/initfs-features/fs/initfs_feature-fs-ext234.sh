initfs_feature_fs_ext_all() {
	initfs_feature_fs_ext2
	initfs_feature_fs_ext3
	initfs_feature_fs_ext4
}

_initfs_feature_fs_ext_all_modules() {
	_initfs_feature_ext2_modules
	_initfs_feature_ext4_modules
}

initfs_feature_fs_ext2() { return 0 ; }

_initfs_feature_fs_ext2_modules() {
	cat <<-EOF
		kernel/fs/ext2/
	EOF
}


initfs_feature_fs_ext3() { return 0 ; }

_initfs_feature_fs_ext3_modules() {
	_initfs_feature_fs_ext4_modules
}


initfs_feature_fs_ext4() { return 0 ; }

_initfs_feature_fs_ext4_modules() {
	cat <<-EOF
		kernel/fs/ext4/
	EOF
}

