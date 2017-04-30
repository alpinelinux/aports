
initfs_feature_crypto_all() { return 0 ; }

_initfs_feature_crypto_all_modules() {
	cat <<-'EOF'
		kernel/crypto/
		kernel/arch/*/crypto/
	EOF
}

