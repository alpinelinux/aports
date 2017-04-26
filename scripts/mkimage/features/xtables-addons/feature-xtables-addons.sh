# Feature for xtables-addons support.

feature_xtables_addons() {
	if kernel_flavors_has "grsec" ; then
		add_initfs_apks "xtables-addons-grsec"
		add_apks "xtables-addons"
	else
		echo "WARNING: Feature_xtables_addons is not currently supported any of your enabled kernel flavors ($(get_kernel_flavors))."
	fi
}
