Entry point scripts:

- `bootstrap.sh`: bootstrap a cross-compilation toolchain.
- `mkimage.sh`: main entry point script for creating tarballs and images.
- `genrootfs.sh`: generate a rootfs based on a list of packages.
- `genapkovl-*.sh`: generate apkovl for specific profiles.
- `mkimage-yaml.sh`: generate `latest-releases.yaml` with release metadata.

Internal helpers, not intended to be invoked directly:

- `mkimg.base.sh`: shared common logic.
- `mkimg.*.sh`: declare individual profiles.
