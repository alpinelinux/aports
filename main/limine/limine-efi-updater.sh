#!/bin/sh
# TODO: limine-enroll-config with b2sum option

. /etc/limine/limine-efi-updater.conf

if [ -z "$efi_file" ]; then
  case "$(uname -m)" in
    aarch64) efi_file="BOOTAA64.EFI" ;;
    loongarch64) efi_file="BOOTLOONGARCH64.EFI" ;;
    riscv64) efi_file="BOOTRISCV64.EFI" ;;
    x86_64|i?86) efi_file="BOOTX64.EFI BOOTIA32.EFI" ;;
    *)
      echo "* could not autodetect EFI file! must set efi_file variable" >&2
      exit 1
  esac
fi

if [ -z "$efi_system_partition" ]; then
  echo "* efi_system_partition variable not set in /etc/limine/limine-efi-updater.conf" >&2
  exit 1
fi

if [ -z "$destination_path" ]; then
  destination_path=/EFI/BOOT
fi

if [ -z "$destination_filename" ]; then
  destination_filename="$efi_file"
fi

if ! [ "$(echo "$efi_file" | wc -w)" = "$(echo "$destination_filename" | wc -w)" ]; then
  echo "* the efi_file variable and the destination_filename variable have a different" >&2
  echo "* amount of items" >&2
  exit 1
fi

for f in $efi_file; do
  if ! [ -f "/usr/share/limine/$f" ]; then
    # not found as a file..
    echo "* efi_file: $f was not found in /usr/share/limine/ .." >&2
    echo "* you probably need to install the package that contains the one you want:" >&2
    echo "* limine-efi-<architecture>" >&2
    exit 1
  fi
done

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

for f in $efi_file; do
  install -Dm644 /usr/share/limine/"$f" "$efi_system_partition"/"$destination_path"/"${destination_filename%% *}"
  destination_filename="${destination_filename#* }"
done
