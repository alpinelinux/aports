# Feature for xtables-addons support.

feature_xtables_addons() {
	if [ "$FLAVOR" = "grsec" ] ; then
		apks_initfs_flavored="$apks_initfs_flavored xtables-addons"
		apks="$apks xtables-addons"
	else
		echo "WARNING: Feature_xtables_addons is not currently supported kernel flavor $FLAVOR."
	fi
}
