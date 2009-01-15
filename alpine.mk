#!/usr/bin/make -f

ISO		?= alpine-test.iso
DESTDIR		?= $(shell pwd)/isotmp
APKDIRS		?= ../aports/core/*/

ISO_DIR		:= $(DESTDIR)/isofs

find_apk	= $(firstword $(wildcard $(addprefix $(APKDIRS),$(1)-[0-9]*.apk)))

KERNEL_APK	:= $(call find_apk,linux-grsec)
MODULE_APK	:= $(wildcard $(subst /linux-grsec-,/linux-grsec-mod-,$(KERNEL_APK)))
KERNEL		:= $(word 3,$(subst -, ,$(notdir $(KERNEL_APK))))-$(word 2,$(subst -, ,$(notdir $(KERNEL_APK))))

ALPINEBASELAYOUT_APK := $(call find_apk,alpine-baselayout)
UCLIBC_APK	:= $(call find_apk,uclibc)
BUSYBOX_APK	:= $(call find_apk,busybox)
APK_TOOLS_APK	:= $(call find_apk,apk-tools)

SOURCE_APKS	:= $(wildcard $(APKDIRS)/*apk)
APK_BIN		:= $(shell which apk)

ifneq ($(words $(KERNEL_APK) $(MODULE_APK) $(ALPINEBASELAYOUT_APK) $(UCLIBC_APK) $(BUSYBOX_APK) $(APK_TOOLS_APK)),6)
$(error I did not find all APKs which I need.)
endif

all: $(ISO)

help:
	@echo "Alpine ISO builder"
	@echo
	@echo "Type 'make' to build $(ISO)"
	@echo
	@echo "I will use the following sources files:"
	@echo " 1. $(notdir $(KERNEL_APK)) (looks like $(KERNEL))"
	@echo " 2. $(notdir $(MODULE_APK))"
	@echo " 3. $(notdir $(ALPINEBASELAYOUT_APK))"
	@echo " 4. $(notdir $(UCLIBC_APK))"
	@echo " 5. $(notdir $(BUSYBOX_APK))"
ifeq ($(APK_BIN),)
	@echo " 6. $(notdir $(APK_TOOLS_APK))"
else
	@echo " 6. $(APK_BIN)"
endif
	@echo

clean:
	rm -rf $(MODLOOP) $(MODLOOP_DIR) $(MODLOOP_DIRSTAMP) \
		$(INITFS) $(INITFS_DIRSTAMP) $(INITFS_DIR) \
		$(ISO_DIR)

#
# Modloop
#
MODLOOP		:= $(ISO_DIR)/boot/modloop.cmg
MODLOOP_DIR	:= $(DESTDIR)/modloop
MODLOOP_DIRSTAMP := $(DESTDIR)/stamp.modloop

$(MODLOOP_DIRSTAMP): $(MODULE_APK)
	@echo "==> modloop: prepare modules $(notdir $(MODULE_APK))"
	@rm -rf $(MODLOOP_DIR)
	@mkdir -p $(MODLOOP_DIR)/lib/modules/
	@tar -C $(MODLOOP_DIR) -xzf $(MODULE_APK)
	@rm -rf $(addprefix $(MODLOOP_DIR)/lib/modules/*/, source build)
	@depmod $(KERNEL) -b $(MODLOOP_DIR)
	@touch $(MODLOOP_DIRSTAMP)

$(MODLOOP): $(MODLOOP_DIRSTAMP)
	@echo "==> modloop: building image $(notdir $(MODLOOP))"
	@mkdir -p $(dir $(MODLOOP))
	@mkcramfs $(MODLOOP_DIR)/lib $(MODLOOP)

#
# Initramfs rules
#

INITFS		:= $(ISO_DIR)/boot/initramfs.gz

INITFS_DIRSTAMP	:= $(DESTDIR)/stamp.initfs
INITFS_DIR	:= $(DESTDIR)/initfs
INITFS_MODDIR	:= $(INITFS_DIR)/lib/modules/$(KERNEL)
INITFS_MODDIRSTAMP := $(DESTDIR)/stamp.initfs.modules

INITFS_APKS	:= $(UCLIBC_APK) $(BUSYBOX_APK)
INITFS_RAWBASEFILES  := etc/mdev.conf etc/passwd etc/group etc/fstab etc/modules
INITFS_BASEFILES     := $(addprefix $(INITFS_DIR)/, $(INITFS_RAWBASEFILES))

$(INITFS_DIRSTAMP): $(INITFS_APKS)
	@echo "==> initramfs: prepare baselayout"
	@rm -rf $(INITFS_DIR)
	@mkdir -p $(addprefix $(INITFS_DIR)/, \
		dev proc sys sbin bin .modloop lib/modules \
		media/cdrom media/floppy media/usb newroot)
	@for apk in $(INITFS_APKS) ; do \
		tar -C $(INITFS_DIR) -xzf $$apk ; \
	done
	@rm -f "$(INITFS_DIR)/.PKGINFO"
	@mknod $(INITFS_DIR)/dev/null c 1 3
	@touch $(INITFS_DIRSTAMP)

$(INITFS_BASEFILES): $(INITFS_DIRSTAMP) $(ALPINEBASELAYOUT_APK)
	@echo "==> initramfs: $(notdir $(ALPINEBASELAYOUT_APK))"
	@tar -C $(INITFS_DIR) -xzf $(ALPINEBASELAYOUT_APK) $(INITFS_RAWBASEFILES)
	@touch $(INITFS_BASEFILES)

$(INITFS_DIR)/init: initramfs-init $(INITFS_DIRSTAMP)
	@echo "==> initramfs: init script"
	@cp initramfs-init "$(INITFS_DIR)/init"

ifeq ($(APK_BIN),)
$(INITFS_DIR)/sbin/apk: $(APK_TOOLS_APK) $(INITFS_DIRSTAMP)
	@echo "==> initramfs: $(notdir $(APK_TOOLS_APK))"
	@tar -C $(INITFS_DIR) -xzf $(APK_TOOLS_APK) sbin/apk
else
$(INITFS_DIR)/sbin/apk: $(APK_BIN) $(INITFS_DIRSTAMP)
	@echo "==> initramfs: copy $(APK_BIN) from buildroot"
	@cp $(APK_BIN) "$(INITFS_DIR)/sbin"
endif

$(INITFS_MODDIRSTAMP): $(INITFS_DIRSTAMP) $(INITFS_MODFILES) $(MODLOOP_DIRSTAMP)
	@echo "==> initramfs: $(notdir $(MODULE_APK))"
	@rm -rf $(INITFS_DIR)/lib/modules
	@mkdir -p $(addprefix $(INITFS_MODDIR)/kernel/,drivers fs)
	@for i in acpi ata block ide scsi cdrom usb message hid; do \
		cp -flLpR $(MODLOOP_DIR)/lib/modules/*/kernel/drivers/$$i \
			$(INITFS_MODDIR)/kernel/drivers/ ; \
	done
	@for i in isofs vfat nls ext2 cramfs '*.ko'; do \
		cp -flLpR $(MODLOOP_DIR)/lib/modules/*/kernel/fs/$$i \
			$(INITFS_MODDIR)/kernel/fs/ ; \
	done
	@cp -flLpR $(MODLOOP_DIR)/lib/modules/*/kernel/lib \
		$(INITFS_MODDIR)/kernel/
	@depmod $(KERNEL) -b $(INITFS_DIR)
	@touch $(INITFS_MODDIRSTAMP)

$(INITFS): $(INITFS_DIRSTAMP) $(INITFS_DIR)/init $(INITFS_DIR)/sbin/apk $(INITFS_MODDIRSTAMP) $(INITFS_BASEFILES)
	@echo "==> initramfs: creating $(notdir $(INITFS))"
	@(cd $(INITFS_DIR) && find . | cpio -o -H newc | gzip -9) > $(INITFS)

#
# ISO rules
#

ISOLINUX	:= $(ISO_DIR)/isolinux
ISOLINUX_BIN	:= $(ISOLINUX)/isolinux.bin
ISOLINUX_CFG	:= $(ISOLINUX)/isolinux.cfg

$(ISOLINUX_BIN): /usr/share/syslinux/isolinux.bin
	@echo "==> iso: install isolinux"
	@mkdir -p $(dir $(ISOLINUX_BIN))
	@cp /usr/share/syslinux/isolinux.bin $(ISOLINUX_BIN)

$(ISOLINUX_CFG):
	@echo "==> iso: configure isolinux"
	@mkdir -p $(dir $(ISOLINUX_BIN))
	@echo "timeout 20" >$(ISOLINUX_CFG)
	@echo "prompt 1" >>$(ISOLINUX_CFG)
	@echo "default linux" >>$(ISOLINUX_CFG)
	@echo "label linux" >>$(ISOLINUX_CFG)
	@echo "	kernel /boot/vmlinuz" >>$(ISOLINUX_CFG)
	@echo "	append initrd=/boot/initramfs.gz alpine_dev=cdrom modules=floppy quiet" >>$(ISOLINUX_CFG)

ISO_KERNEL	:= $(ISO_DIR)/boot/vmlinuz
ISO_APKS	:= $(ISO_DIR)/apks
ISO_APKINDEX	:= $(ISO_APKS)/APK_INDEX.gz

$(ISO_APKS): $(SOURCE_APKS)
	@echo "==> iso: prepare APK repository"
	@rm -rf $(ISO_APKS)
	@mkdir -p $(ISO_APKS)
	@for a in $(SOURCE_APKS) ; do \
		ln -f $$a $(ISO_APKS) 2>/dev/null || cp $$a $(ISO_APKS) ; \
	done
	@apk index $(SOURCE_APKS) | gzip -9 > $(ISO_APKINDEX)

$(ISO_KERNEL): $(KERNEL_APK)
	@echo "==> iso: install kernel $(KERNEL)"
	@mkdir -p $(dir $(ISO_KERNEL))
	@tar -C $(ISO_DIR) -xzf $(KERNEL_APK) boot/vmlinuz boot/System.map
	@touch $(ISO_KERNEL)

$(ISO): $(MODLOOP) $(INITFS) $(ISOLINUX_CFG) $(ISOLINUX_BIN) $(ISO_KERNEL) $(ISO_APKS)
	@echo "==> iso: building $(notdir $(ISO))"
	@genisoimage -o $(ISO) -l -J -R \
		-b isolinux/isolinux.bin \
		-c isolinux/boot.cat	\
		-no-emul-boot		\
		-boot-load-size 4	\
		-boot-info-table	\
		-quiet			\
		$(ISO_DIR)

