
initfs_crypto_all() {
	return 0
}

initfs_crypto_all_modules() {
	cat <<-'EOF'
		kernel/crypto/
		kernel/arch/*/crypto/
	EOF
}

_initfs_crypto_all_files() { return 0 ; }
