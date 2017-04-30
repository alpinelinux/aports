initfs_feature_dm_crypsetup() { return 0 ; }

_initfs_feature_dm_crypsetup_modules() {
	_initfs_feature_crypto_all_modules
	cat <<-'EOF'
		dm-crypt
	EOF
}

_initfs_feature_dm_crypsetup_pkgs() {
	cat <<-'EOF'
		cryptsetup
	EOF
}

_initfs_feature_dm_crypsetup_files() {
	cat <<-'EOF'
		/sbin/cryptsetup
	EOF
}

