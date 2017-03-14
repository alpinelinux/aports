# Support for the PostGIS geospatial extensions to PostgreSQL.
feature_postgis() {
	feature_postgresql "$@"
	add_apks "postgis"
	[ "$postgresql_autostart" = "true" ] && add_rootfs_apks "postgis"

	add_overlays "postgis"
}

# Furture home of logic to setup postgis enabled template DB and autoload GIS data.
overlay_postgis() {
	return 0
}


