
profile_fileserver() {
	iso_label="alp-fserv $RELEASE $ARCH"
	profile_standard
	feature_nfs
	feature_cifs
	feature_samba
}
