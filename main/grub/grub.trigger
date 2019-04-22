#!/bin/sh
if [ -e /boot/grub/grub.cfg ]; then
	cp /boot/grub/grub.cfg /boot/grub/grub.cfg.backup
fi
mkdir -p /boot/grub
grub-mkconfig -o /boot/grub/grub.cfg.new \
	&& mv /boot/grub/grub.cfg.new /boot/grub/grub.cfg

