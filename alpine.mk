#!/usr/bin/make -f

PROFILE		?= alpine

-include $(PROFILE).conf.mk

BUILD_DATE	:= $(shell date +%y%m%d)
ALPINE_RELEASE	?= $(BUILD_DATE)
ALPINE_NAME	?= alpine-test
ALPINE_ARCH	:= x86
DESTDIR		?= $(shell pwd)/isotmp.$(PROFILE)

MKCRAMFS	= mkcramfs
SUDO		= sudo

ISO		?= $(ALPINE_NAME)-$(ALPINE_RELEASE)-$(ALPINE_ARCH).iso
ISO_LINK	?= $(ALPINE_NAME).iso
ISO_DIR		:= $(DESTDIR)/isofs
ISO_PKGDIR	:= $(ISO_DIR)/apks

APKS		?= $(shell sed 's/\#.*//; s/\*/\\*/g' $(PROFILE).packages)

APK_OPTS	:= $(addprefix --repository ,$(APK_REPOS)) --keys-dir /etc/apk/keys

find_apk_ver	= $(shell apk search $(APK_OPTS) $(1) | sort | uniq)
find_apk_file	= $(addsuffix .apk,$(call find_apk_ver,$(1)))
find_apk	= $(addprefix $(ISO_PKGDIR)/,$(call find_apk_file,$(1)))

# get apk does not support wildcards
get_apk         = $(addsuffix .apk,$(shell apk fetch --simulate $(APK_OPTS) $(1) 2>&1 | sed 's:^Downloading :$(ISO_PKGDIR)/:'))

KERNEL_FLAVOR	?= grsec
KERNEL_PKGNAME	?= linux-$(KERNEL_FLAVOR)
KERNEL_NAME	:= $(KERNEL_FLAVOR)
KERNEL_APK	:= $(call get_apk,$(KERNEL_PKGNAME))

KERNEL		:= $(word 3,$(subst -, ,$(notdir $(KERNEL_APK))))-$(word 2,$(subst -, ,$(notdir $(KERNEL_APK))))

ALPINEBASELAYOUT_APK := $(call find_apk,alpine-baselayout)
UCLIBC_APK	:= $(call get_apk,uclibc)
BUSYBOX_APK	:= $(call get_apk,busybox)
APK_TOOLS_APK	:= $(call get_apk,apk-tools)
STRACE_APK	:= $(call get_apk,strace)

APKS_FILTER	?= | grep -v -- '-dev$$' | grep -v 'sources'

APKS		?= '*'
APK_FILES	:= $(call find_apk,$(APKS))

all: isofs

help:
	@echo "Alpine ISO builder"
	@echo
	@echo "Type 'make iso' to build $(ISO)"
	@echo
	@echo "I will use the following sources files:"
	@echo " 1. $(notdir $(KERNEL_APK)) (looks like $(KERNEL))"
	@echo " 2. $(notdir $(MOD_APKS))"
	@echo " 3. $(notdir $(ALPINEBASELAYOUT_APK))"
	@echo " 4. $(notdir $(UCLIBC_APK))"
	@echo " 5. $(notdir $(BUSYBOX_APK))"
ifeq ($(APK_BIN),)
	@echo " 6. $(notdir $(APK_TOOLS_APK))"
else
	@echo " 6. $(APK_BIN)"
endif
	@echo
	@echo "ALPINE_NAME:    $(ALPINE_NAME)"
	@echo "ALPINE_RELEASE: $(ALPINE_RELEASE)"
	@echo "KERNEL_FLAVOR:  $(KERNEL_FLAVOR)"
	@echo "KERNEL:         $(KERNEL)"
	@echo

clean:
	rm -rf $(MODLOOP) $(MODLOOP_DIR) $(MODLOOP_DIRSTAMP) \
		$(INITFS) $(INITFS_DIRSTAMP) $(INITFS_DIR) \
		$(ISO_DIR) $(ISO_REPOS_DIRSTAMP)


$(APK_FILES):
	@mkdir -p "$(dir $@)";\
	p="$(notdir $(basename $@))";\
	apk fetch $(APK_REPO) -R -v -o "$(dir $@)" $${p%-[0-9]*}
#	apk fetch $(APK_OPTS) -R -v -o "$(dir $@)" \
#		`apk search -q $(APK_OPTS) $(APKS) | sort | uniq`

#
# Modloop
#
MODLOOP		:= $(ISO_DIR)/boot/$(KERNEL_NAME).cmg
MODLOOP_DIR	:= $(DESTDIR)/modloop
MODLOOP_KERNELSTAMP := $(DESTDIR)/stamp.modloop.kernel
MODLOOP_DIRSTAMP := $(DESTDIR)/stamp.modloop
MODLOOP_EXTRA	?= $(addsuffix -$(KERNEL_FLAVOR), dahdi-linux iscsitarget xtables-addons)
MODLOOP_PKGS	= $(KERNEL_PKGNAME) $(MODLOOP_EXTRA)

modloop: $(MODLOOP)

$(MODLOOP_KERNELSTAMP):
	@echo "==> modloop: Unpacking kernel modules";
	@rm -rf $(MODLOOP_DIR)
	@mkdir -p $(MODLOOP_DIR)/lib/modules/
	@for i in $(MODLOOP_PKGS); do \
		apk fetch $(APK_OPTS) --stdout $$i \
			| tar -C $(MODLOOP_DIR) -xz; \
	done
	@cp $(MODLOOP_DIR)/usr/share/kernel/$(KERNEL_FLAVOR)/kernel.release $@

MODLOOP_KERNEL_RELEASE = $(shell cat $(MODLOOP_KERNELSTAMP))

$(MODLOOP_DIRSTAMP): $(MODLOOP_KERNELSTAMP)
	@rm -rf $(addprefix $(MODLOOP_DIR)/lib/modules/*/, source build)
	@depmod $(MODLOOP_KERNEL_RELEASE) -b $(MODLOOP_DIR)
	@touch $(MODLOOP_DIRSTAMP)

$(MODLOOP): $(MODLOOP_DIRSTAMP)
	@echo "==> modloop: building image $(notdir $(MODLOOP))"
	@mkdir -p $(dir $(MODLOOP))
	@$(MKCRAMFS) $(MODLOOP_DIR)/lib $(MODLOOP)

clean-modloop:
	@rm -rf $(MODLOOP_DIR) $(MODLOOP_DIRSTAMP) $(MODLOOP_PKGSTAMP) $(MODLOOP)

#
# Initramfs rules
#

# isolinux cannot handle - in filenames
#INITFS_NAME	:= initramfs-$(MODLOOP_KERNEL_RELEASE)
INITFS_NAME	:= $(KERNEL_FLAVOR).gz
INITFS		:= $(ISO_DIR)/boot/$(INITFS_NAME)

INITFS_DIR	:= $(DESTDIR)/initfs
INITFS_TMP	:= $(DESTDIR)/tmp.initfs
INITFS_DIRSTAMP := $(DESTDIR)/stamp.initfs
INITFS_FEATURES	:= ata base bootchart cdrom cramfs ext3 floppy raid scsi usb
INITFS_PKGS	:= $(MODLOOP_PKGS) alpine-base acct

initfs: $(INITFS)

$(INITFS_DIRSTAMP):
	@rm -rf $(INITFS_DIR) $(INITFS_TMP)
	@mkdir -p $(INITFS_DIR) $(INITFS_TMP)
	@for i in `apk fetch $(APK_OPTS) --simulate -R $(INITFS_PKGS) 2>&1\
			| sed 's:^Downloading ::; s:-[0-9].*::' | sort | uniq`; do \
		apk fetch $(APK_OPTS) --stdout $$i \
			| tar -C $(INITFS_DIR) -zx || exit 1; \
	done
	@cp -r /etc/apk/keys $(INITFS_DIR)/etc/apk/ || true
	@touch $@

#$(INITFS):	$(shell mkinitfs -F "$(INITFS_FEATURES)" -l $(KERNEL))
$(INITFS): $(INITFS_DIRSTAMP) $(MODLOOP_DIRSTAMP)
	@mkinitfs -F "$(INITFS_FEATURES)" -t $(INITFS_TMP) \
		-b $(INITFS_DIR) -o $@ $(MODLOOP_KERNEL_RELEASE)

clean-initfs:
	@rm -rf $(INITFS) $(INITFS_DIRSTAMP) $(INITFS_DIR)

#
# Vserver template rules
#
VSTEMPLATE	:= $(ISO_DIR)/vs-template.tar.bz2
VSTEMPLATE_DIR 	:= $(DESTDIR)/vs-template

vstemplate: $(VSTEMPLATE)
	@echo "==> vstemplate: built $(VSTEMPLATE)"

#must be run as root or in fakeroot
$(VSTEMPLATE):
	@rm -rf "$(VSTEMPLATE_DIR)"
	@mkdir -p "$(VSTEMPLATE_DIR)"
	@apk add $(APK_OPTS) --initdb --root $(VSTEMPLATE_DIR) \
		alpine-base
	@cd $(VSTEMPLATE_DIR) && tar -jcf $@ *

#
# ISO rules
#

ISOLINUX_DIR	:= boot/isolinux
ISOLINUX	:= $(ISO_DIR)/$(ISOLINUX_DIR)
ISOLINUX_BIN	:= $(ISOLINUX)/isolinux.bin
ISOLINUX_CFG	:= $(ISOLINUX)/isolinux.cfg
SYSLINUX_CFG	:= $(ISO_DIR)/syslinux.cfg

$(ISOLINUX_BIN):
	@echo "==> iso: install isolinux"
	@mkdir -p $(dir $(ISOLINUX_BIN))
	@if ! apk fetch $(APK_REPO) --stdout syslinux | tar -O -zx usr/share/syslinux/isolinux.bin > $@; then \
		rm -f $@ && exit 1;\
	fi

$(ISOLINUX_CFG):
	@echo "==> iso: configure isolinux"
	@mkdir -p $(dir $(ISOLINUX_BIN))
	@echo "timeout 20" >$(ISOLINUX_CFG)
	@echo "prompt 1" >>$(ISOLINUX_CFG)
	@echo "default $(KERNEL_NAME)" >>$(ISOLINUX_CFG)
	@echo "label $(KERNEL_NAME)" >>$(ISOLINUX_CFG)
	@echo "	kernel /boot/$(KERNEL_NAME)" >>$(ISOLINUX_CFG)
	@echo "	append initrd=/boot/$(INITFS_NAME) alpine_dev=cdrom:iso9660 modules=loop,cramfs,sd-mod,usb-storage,floppy quiet" >>$(ISOLINUX_CFG)

$(SYSLINUX_CFG): $(MODLOOP_DIRSTAMP)
	@echo "==> iso: configure syslinux"
	@echo "timeout 20" >$@
	@echo "prompt 1" >>$@
	@echo "default $(KERNEL_NAME)" >>$@
	@echo "label $(KERNEL_NAME)" >>$@
	@echo "	kernel /boot/$(KERNEL_NAME)" >>$@
	@echo "	append initrd=/boot/$(INITFS_NAME) alpine_dev=usbdisk:vfat modules=loop,cramfs,sd-mod,usb-storage quiet" >>$@

clean-syslinux:
	@rm -f $(SYSLINUX_CFG) $(ISOLINUX_CFG) $(ISOLINUX_BIN)

ISO_KERNEL	:= $(ISO_DIR)/boot/$(KERNEL_NAME)
ISO_REPOS_DIRSTAMP := $(DESTDIR)/stamp.isorepos
ISOFS_DIRSTAMP	:= $(DESTDIR)/stamp.isofs

$(ISO_REPOS_DIRSTAMP): $(ISO_PKGDIR)/APKINDEX.tar.gz
	@touch $(ISO_PKGDIR)/.boot_repository
	@rm -f $(ISO_PKGDIR)/.SIGN.*
	@touch $@

$(ISO_PKGDIR)/APKINDEX.tar.gz: $(APK_FILES)
	@echo "==> iso: generating repository index"
	@apk index --description "$(ALPINE_NAME) $(ALPINE_RELEASE)" \
		-o $@ $(ISO_PKGDIR)/*.apk
	@abuild-sign $@

$(ISO_KERNEL): $(MODLOOP_DIRSTAMP)
	@echo "==> iso: install kernel $(KERNEL)"
	@mkdir -p $(dir $(ISO_KERNEL))
	@apk fetch $(APK_OPTS) --stdout $(KERNEL_PKGNAME) \
		| tar -C $(ISO_DIR) -xz boot
	@rm -f $(ISO_DIR)/boot/$(KERNEL_NAME)
	@ln -s vmlinuz-$(MODLOOP_KERNEL_RELEASE) $@
	@rm -rf $(ISO_DIR)/.[A-Z]* $(ISO_DIR)/.[a-z]* $(ISO_DIR)/lib

$(ISOFS_DIRSTAMP): $(MODLOOP) $(INITFS) $(ISOLINUX_CFG) $(ISOLINUX_BIN) $(ISO_KERNEL) $(ISO_REPOS_DIRSTAMP) $(SYSLINUX_CFG)
	@echo "$(ALPINE_NAME)-$(ALPINE_RELEASE) $(BUILD_DATE)" \
		> $(ISO_DIR)/.alpine-release
	@touch $@

$(ISO): $(ISOFS_DIRSTAMP)
	@echo "==> iso: building $(notdir $(ISO))"
	@genisoimage -o $(ISO) -l -J -R \
		-b $(ISOLINUX_DIR)/isolinux.bin \
		-c $(ISOLINUX_DIR)/boot.cat	\
		-no-emul-boot		\
		-boot-load-size 4	\
		-boot-info-table	\
		-quiet			\
		-follow-links		\
		$(ISO_OPTS)		\
		$(ISO_DIR)
	@ln -fs $@ $(ISO_LINK)

isofs: $(ISOFS_DIRSTAMP)
iso: $(ISO)

#
# SHA1 sum of ISO
#
ISO_SHA1	:= $(ISO).sha1

$(ISO_SHA1):	$(ISO)
	@echo "==> Generating sha1 sum"
	@sha1sum $(ISO) > $@ || rm -f $@

#
# USB image
#
USBIMG 		:= $(ALPINE_NAME)-$(ALPINE_RELEASE)-$(ALPINE_ARCH).img
USBIMG_FREE	?= 8192
USBIMG_SIZE 	= $(shell echo $$(( `du -s $(ISO_DIR) | awk '{print $$1}'` + $(USBIMG_FREE) )) )
MBRPATH 	:= /usr/share/syslinux/mbr.bin

$(USBIMG): $(ISOFS_DIRSTAMP)
	@echo "==> Generating $@"
	@mformat -C -v 'ALPINE' -c 16 -h 64 -n 32 -i $(USBIMG) \
		-t $$(($(USBIMG_SIZE) / 1000)) ::
	@syslinux $(USBIMG)
	@mcopy -i $(USBIMG) $(ISO_DIR)/* $(ISO_DIR)/.[a-z]* ::
	@mcopy -i $(USBIMG) /dev/zero ::/zero 2>/dev/null || true
	@mdel -i $(USBIMG) ::/zero

USBIMG_SHA1	:= $(USBIMG).sha1
$(USBIMG_SHA1):	$(USBIMG)
	@echo "==> Generating sha1 sum"
	@sha1sum $(USBIMG) > $@ || rm -f $@

$(ALPINE_NAME).img:	$(USBIMG)
	@ln -sf $(USBIMG) $@

img:	$(ALPINE_NAME).img

sha1: $(ISO_SHA1) $(USBIMG_SHA1)

release: $(ISO_SHA1)

