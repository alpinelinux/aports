WandBoard
---------

- ROM loads boot loader from raw MMC sectors at fixed address
- NOTE: 1st partition needs to start after boot loader

- Install u-boot with (pick the command for your board type):
  dd if=wandboard_solo/u-boot.imx of=/dev/mmcblk0 seek=1 conv=fsync bs=1k
  dd if=wandboard_dl/u-boot.imx   of=/dev/mmcblk0 seek=1 conv=fsync bs=1k
  dd if=wandboard_quad/u-boot.imx of=/dev/mmcblk0 seek=1 conv=fsync bs=1k


BeagleBoard
-----------

- ROM looks for 1st partition with FAT, and loads MLO from it
- NOTE: MLO needs to be the first file created on this partition

- Install u-boot with:
  cp am335x_boneblack/{MLO,u-boot.img} /media/mmcblk0p1/

