#!/bin/sh

. /etc/limine/limine-bios-updater.conf

if [ "$disable_hook" = 1 ]; then
  exit 0
fi

exec /usr/bin/limine-bios-updater.sh
