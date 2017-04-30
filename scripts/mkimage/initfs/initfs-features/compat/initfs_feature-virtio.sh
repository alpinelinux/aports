# Initifs feature virtio.

initfs_feature_virtio() { return 0 ; }

_initfs_feature_virtio_modules() {
	cat <<-'EOF'
		kernel/drivers/block/virtio*
		kernel/drivers/virtio/
		kernel/drivers/net/virtio_net*
	EOF
}

