create_image_targz() {
	tar -C "${DESTDIR}" -chzf ${OUTDIR}/${output_filename} .
}
