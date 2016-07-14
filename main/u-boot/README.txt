WandBoard
---------

- ROM loads boot loader from raw MMC sectors at fixed address
- NOTE: 1st partition needs to start after boot loader

- Install u-boot with:
  dd if=wandboard/SPL of=/dev/mmcblk0 bs=1k seek=1
  dd if=wandboard/u-boot.img of=/dev/mmcblk0 bs=1k seek=69
  sync

  (Note - the SD card node may vary, so adjust this as needed).

- Insert the SD card into the slot located in the bottom of the board
  (same side as the mx6 processor)

BeagleBoard
-----------

- ROM looks for 1st partition with FAT, and loads MLO from it
- NOTE: MLO needs to be the first file created on this partition

- Install u-boot with:
  cp am335x_boneblack/{MLO,u-boot.img} /media/mmcblk0p1/

Sunxi (Cubie* etc)
------------------

- ROM loads boot loader from SD-CARD sectors at fixed address
- Install u-boot with:
    sudo dd if=<board>/u-boot-sunxi-with-spl.bin of=/dev/sda bs=1024 seek=8

