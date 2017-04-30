# Initifs feature lvm.

initfs_feature_lvm() { return 0 ; }

_initfs_feature_lvm_modules() {
	cat <<-'EOF'
		kernel/drivers/md/dm-mod.ko
		kernel/drivers/md/dm-snapshot.ko
	EOF
}

_initfs_feature_lvm_pkgs() {
	cat <<-'EOF'
		lvm2
	EOF
}

_initfs_feature_lvm_files() {
	cat <<-'EOF'
		/sbin/lvm
	EOF
}
