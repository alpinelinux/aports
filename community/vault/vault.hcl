/*
 * Vault configuration. See: https://vaultproject.io/docs/config/
 */

backend "file" {
	path = "/var/lib/vault"
}

listener "tcp" {
	/*
	 * By default Vault listens on localhost only.
	 * Make sure to enable TLS support otherwise.
	 */
	tls_disable = 1
}
