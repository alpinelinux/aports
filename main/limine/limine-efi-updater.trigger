#!/bin/sh

. /etc/limine/limine-efi-updater.conf

if [ "$disable_hook" = 1 ]; then
  exit 0
fi

exec /usr/bin/limine-efi-updater.sh
