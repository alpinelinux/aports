initfs_crypto_crc32_modules() {
	return 0
}

_initfs_crypto_crc32_modules() {
	cat <<-'EOF'
		kernel/arch/*/crypto/crc32*
		kernel/crypto/crc32*
	EOF
}

_initfs_crypto_crc32_files() { return 0 ; }
