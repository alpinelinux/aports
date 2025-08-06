/*
 * OpenBao configuration. See: https://openbao.org/docs/configuration
 */

storage "file" {
	path = "/var/lib/openbao"
}

listener "tcp" {
	/*
	 * By default OpenBao listens on localhost only.
	 * Make sure to enable TLS support otherwise.
	 */
	tls_disable = 1
}

api_addr = "http://127.0.0.1:8200"
