initfs_feature_virt_virtio() { return 0 ; }

_initfs_feature_virt_virtio_modules() {
	cat <<-'EOF'
		kernel/drivers/block/virtio*
		kernel/drivers/virtio/
		kernel/drivers/net/virtio_net*
	EOF
}

