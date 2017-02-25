# Feature to support both kvm hosts and guests.
# Use feature_kvm_host or feature_kvm_guest independently if desired.

feature_kvm() {
	feature_kvm_host
	feature_kvm_guest
}


feature_kvm_host() {
	apks="$apks qemu qemu-img qemu-openrc qemu-system-$ARCH"
}


feature_kvm_guest() {
	initfs_features="$initfs_features virtio"
}
