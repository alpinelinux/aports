initfs_virt_guest_drivers_compat_all() {
	return 0
}

_initfs_virt_guest_drivers_compat_all_modules() {
	_initfs_virt_guest_drivers_compat_net_modules
	_initfs_virt_guest_drivers_compat_ata_modules
	_initfs_virt_guest_drivers_compat_scsi_modules
}

_initfs_virt_guest_drivers_compat_all_files() {
	_initfs_virt_guest_drivers_compat_net_files
	_initfs_virt_guest_drivers_compat_ata_files
	_initfs_virt_guest_drivers_compat_scsi_files
}




initfs_virt_guest_drivers_compat_net() {
	return 0
}

_initfs_virt_guest_drivers_compat_net_modules() {
	cat <<-'EOF'
		kernel/drivers/net/virtio_net.ko
		kernel/drivers/net/hyperv/hv_netvsc.ko
		kernel/drivers/net/vmxnet3/vmxnet3.ko
		kernel/drivers/net/ethernet/intel/e1000/e1000.ko
		kernel/drivers/net/ethernet/realtek/rtl8139too.ko
		kernel/drivers/net/ethernet/8390/ne2k_pci.ko
		kernel/drivers/net/ethernet/8390/8390.ko
	EOF
}

_initfs_virt_guest_drivers_compat_net_files() { return 0 ; }




initfs_virt_guest_drivers_compat_ata() {
	return 0
}

_initfs_virt_guest_drivers_compat_ata_modules() {
	cat <<-'EOF'
		kernel/drivers/ata/ahci.ko
		kernel/drivers/ata/libahci.ko
		kernel/drivers/ata/libata.ko
	EOF
}

_initfs_virt_guest_drivers_compat_ata_files() { return 0 ; }




initfs_virt_guest_drivers_compat_scsi() {
	return 0
}

_initfs_virt_guest_drivers_compat_scsi_modules() {
	cat <<-'EOF'
		kernel/drivers/scsi/scsi_mod.ko
		kernel/drivers/scsi/sd_mod.ko
		kernel/drivers/scsi/sr_mod.ko
		kernel/drivers/scsi/sg.koi

		kernel/drivers/scsi/virtio_scsi.ko
		kernel/drivers/scsi/vmw_pvscsi.ko
		kernel/drivers/scsi/xen-scsifront.ko

		kernel/drivers/scsi/libsas/libsas.ko
		kernel/drivers/scsi/ses.ko
		kernel/drivers/scsi/scsi_transport_sas.ko
		kernel/drivers/scsi/megaraid/megaraid_sas.ko
		kernel/drivers/scsi/sym53c8xx_2/sym53c8xx.ko
	EOF
}

_initfs_virt_guest_drivers_compat_scsi_files() { return 0 ; }



