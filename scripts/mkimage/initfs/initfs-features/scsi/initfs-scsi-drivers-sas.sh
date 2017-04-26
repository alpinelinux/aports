initfs_scsi_drivers_sas() {
	return 0
}

_initfs_scsi_drivers_sas_modules() {
	cat <<-'EOF'
		kernel/drivers/scsi/libsas
		kernel/drivers/ata/libata.ko
		kernel/drivers/scsi/scsi_transport_sas.ko
		kernel/drivers/scsi/scsi_mod.ko
		kernel/drivers/scsi/ses.ko

		kernel/drivers/scsi/3w-sas.ko
		kernel/drivers/scsi/isci
		kernel/drivers/scsi/aic94xx/aic94xx.ko
		kernel/drivers/scsi/esas2r/esas2r.ko
		kernel/drivers/scsi/hpsa.ko
		kernel/drivers/scsi/megaraid/megaraid-sas.ko
		kernel/drivers/scsi/mpt3sas/mpt3sas.ko
		kernel/drivers/scsi/mvsas/mvsas.ko
		kernel/drivers/scsi/pm8001/pm80xx.ko
		kernel/drivers/scsi/smartpqi/smarpqi.ko
		kernel/drivers/message/fusion/mptsas.ko
	EOF
}

_initfs_scsi_drivers_sas_files() { return 0 ; }
