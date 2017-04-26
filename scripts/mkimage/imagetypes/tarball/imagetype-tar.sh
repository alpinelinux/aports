imagetype_tar() {
	return 0
}

create_image_tar() {
	tar -C "${DESTDIR}" -chf "${OUTDIR}/${output_filename}" .
}
