initfs_virt_virtio() {
	return 0
}

_initfs_virt_virtio_modules() {
	cat <<-'EOF'
		kernel/drivers/block/virtio*
		kernel/drivers/virtio/
		kernel/drivers/net/virtio_net*
	EOF
}

_initfs_virt_virtio_files() { return 0 ; }
