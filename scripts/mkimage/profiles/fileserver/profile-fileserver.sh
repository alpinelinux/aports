
profile_fileserver() {
	profile_standard
	feature_nfs
	feature_cifs
	feature_samba
	iso_label="alp-fserv $RELEASE $ARCH"
}
