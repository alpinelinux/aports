# Feature for xtables-addons support.

feature_xtables_addons() {
	if kernel_flavors_has "grsec" ; then
		add_initfs_apks_flavored "xtables-addons"
		add_apks "xtables-addons"
	else
		echo "WARNING: Feature_xtables_addons is not currently supported any of your enabled kernel flavor ($(get_kernel_flavors))."
	fi
}
