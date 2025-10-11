#!/bin/sh

. /etc/limine/limine-bios-updater.conf

# partition | mountpoint | fstype | flags | ..
parttype="$(awk '$2 == "/boot" { print $3 }' < /etc/mtab)"

if ! [ -n "$parttype" ] || ! [ "$parttype" = "vfat" ]; then
  echo "* limine-bios-updater expects /boot to be a mounted vfat partition due to" >&2
  echo "* bootloader constraints" >&2
  exit 1
fi

install -Dm644 /usr/share/limine/limine-bios.sys -t /boot/limine/

limine bios-install "$boot_device"
