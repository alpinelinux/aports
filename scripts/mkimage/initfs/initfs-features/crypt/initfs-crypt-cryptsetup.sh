initfs_crypt_crypsetup() {
	return 0
}

initfs_crypt_crypsetup() {
	cat <<-'EOF'
		kernel/crypto/*
		kernel/arch/*/crypto/*
		kernel/drivers/md/dm-crypt.ko
	EOF
}

_initfs_crypt_crypsetup_files() {
	cat <<-'EOF'
		/sbin/cryptsetup
	EOF
}

