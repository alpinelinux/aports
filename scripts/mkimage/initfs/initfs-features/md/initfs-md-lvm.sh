initfs_md_lvm() {
	return 0
}

_initfs_md_lvm_modules() {
	cat <<-'EOF'
		kernel/drivers/md/dm-mod.ko
		kernel/drivers/md/dm-snapshot.ko
	EOF
}

_initfs_md_lvm_files() {
	cat <<-'EOF'
		/sbin/lvm
	EOF
}

