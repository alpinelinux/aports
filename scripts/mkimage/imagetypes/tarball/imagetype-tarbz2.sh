imagetype_tarbz2() {
	return 0
}

create_image_tarbz2() {
	tar -C "${DESTDIR}" -chjf "${OUTDIR}/${output_filename}" .
}
