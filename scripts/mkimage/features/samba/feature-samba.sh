# Features to allow samba to support client/server participation on windows-based (SMB/CIFS) networks (file/print/auth).
# Client/Server enabled by feature_samba
# Enable windbind support and samba domain controller using feature_samba_winbind or feature_samba_dc respectively.

feature_samba() {
	add_apks "samba"
	feature_samba_client
	feature_samba_server
}


feature_samba_client() {
	add_apks "samba-client"
}


feature_samba_server() {
	add_apks "samba-server"
}


feature_samba_winbind() {
	add_apks "samba-winbind"
}


feature_samba_dc() {
	add_apks "samba-dc"
	feature_samba_server
	feature_samba_winbind
}
