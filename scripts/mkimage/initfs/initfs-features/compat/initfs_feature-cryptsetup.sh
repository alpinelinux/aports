# Initifs feature cryptsetup.

initfs_feature_cryptsetup() { return 0 ; }

_initfs_feature_cryptsetup_modules() {
	cat <<-'EOF'
		kernel/crypto/*
		kernel/arch/*/crypto/*
		dm-crypt
	EOF
}

_initfs_feature_cryptsetup_pkgs() {
	cat <<-'EOF'
		cryptsetup
	EOF
}

_initfs_feature_cryptsetup_files() {
	cat <<-'EOF'
		/sbin/cryptsetup
	EOF
}
