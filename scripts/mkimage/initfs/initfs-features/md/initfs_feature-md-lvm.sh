initfs_feature_md_lvm() { return 0 ; }

_initfs_feature_md_lvm_modules() {
	cat <<-'EOF'
		dm-mod
		dm-snapshot
	EOF
}

_initfs_feature_md_lvm_pkgs() {
	cat <<-'EOF'
		lvm2
	EOF
}

_initfs_feature_md_lvm_files() {
	cat <<-'EOF'
		/sbin/lvm
	EOF
}

