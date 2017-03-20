
initfs_crypto_all() {
	return 0
}

initfs_crypto_all() {
	cat <<-'EOF'
		kernel/crypto/
		kernel/arch/*/crypto/
	EOF
}

