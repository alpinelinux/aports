
profile_fileserver() {
	profile_standard
	feature_nfs
	feature_cifs
	feature_samba
	set_iso_label_if_is_empty "alp-fserv $RELEASE $ARCH"
}
