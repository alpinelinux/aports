#!/bin/sh
# TODO: limine-enroll-config with b2sum option

. /etc/limine/limine-efi.conf

if [ "$disable_update_hook" = 1 ]; then
  exit 0
fi

if ! [ -f "/usr/share/limine/$efi_file" ]; then
  # not found as a file..
  echo "* efi_file: $efi_file was not found in /usr/share/limine/ .." >&2
  echo "* you probably need to install the package that contains the one you want:" >&2
  echo "* [limine-x86_64 | limine-x86_32 | limine-aarch64]" >&2
  echo "* and configure efi_file accordingly in /etc/limine-efi.conf" >&2
  echo "*" >&2
  echo "* seeing this on first install is normal." >&2
  exit 1
fi

# partition | mountpoint | fstype | flags | ..
parttype="$(awk "\$2 == \"$efi_system_partition\" { print \$3 }" < /etc/mtab)"

if ! [ -n "$parttype" ]; then
  # didn't find anything
  echo "* could not detect partition from efi_system_partition: $efi_system_partition" >&2
  echo "* are you sure it is set to a mount target?" >&2
  exit 1
elif ! [ "$parttype" = "vfat" ]; then
  # not vfat, so not ESP
  echo "* the configured efi_system_partition: $efi_system_partition" >&2
  echo "* is not detected as vfat. ESP partitions must be FAT!" >&2
  exit 1
fi
# is vfat and correct mountpoint..

# correct location to place a BOOTXXXX.efi that gets default-loaded.
# make the directory in case it doesn't already exist..
mkdir -p "$efi_system_partition"/EFI/BOOT/

install -Dm755 /usr/share/limine/"$efi_file" -t "$efi_system_partition"/EFI/BOOT/
