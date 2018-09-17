#!/bin/sh
if [ -e /boot/grub/grub.cfg ]; then
	if [ -e /boot/vmlinuz-vanilla ]; then
		sed -i -e "s/vmlinuz /vmlinuz-vanilla /g" /boot/grub/grub.cfg
	else
		if [ -e /boot/vmlinuz ]; then
			sed -i -e "s/vmlinuz-vanilla/vmlinuz/g" /boot/grub/grub.cfg
		fi
	fi
fi
