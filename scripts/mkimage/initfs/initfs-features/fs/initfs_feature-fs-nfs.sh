initfs_feature_fs_nfs_all() {
	initfs_feature_fs_nfs
	initfs_feature_fs_nfsd
}



initfs_feature_fs_nfs() { return 0 ; }

_initfs_feature_fs_nfs_modules() {
	cat <<-EOF
		kernel/fs/nfs/
	EOF
}


initfs_feature_fs_nfsd() { return 0 ; }

_initfs_feature_fs_nfsd_modules() {
	cat <<-EOF
		kernel/fs/nfsd/
	EOF
}

