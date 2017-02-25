imagetype_targz() {
	return 0
}

create_image_targz() {
	tar -C "${DESTDIR}" -chzf ${OUTDIR}/${output_filename} .
}
