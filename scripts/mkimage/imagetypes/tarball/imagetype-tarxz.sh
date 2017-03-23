imagetype_tarxz() {
	return 0
}

create_image_tarxz() {
	tar -C "${DESTDIR}" -chJf "${OUTDIR}/${output_filename}" .
}
