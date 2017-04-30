# Initifs feature bootchart.

initfs_feature_bootchart() { return 0 ; }

_initfs_feature_bootchart_files() {
	cat <<-'EOF'
		/sbin/bootchartd
		/usr/bin/ac
		/usr/bin/last
		/usr/bin/lastcomm
		/usr/sbin/dump-utmp
		/usr/sbin/dump-acct
		/usr/sbin/accton
		/usr/sbin/sa
	EOF
}

