
# From xentec 20170420
rpi_install_dtbs() {
	while read -r line ; do
		case "$line" in
			dtoverlay=*)
				line="${line#*=}"
				line="${line#,*}.dtbo"
				install -D "$DTBDIR/overlays/$line" "/boot/overlays/$line"
				;;
		esac
	done < /boot/config.txt

}

