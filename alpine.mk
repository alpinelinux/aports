#!/usr/bin/make -f

-include alpine.conf.mk

BUILD_DATE	:= $(shell date +%y%m%d)
ALPINE_RELEASE	?= $(BUILD_DATE)
ALPINE_NAME	?= alpine-test
ALPINE_ARCH	:= i386
DESTDIR		?= $(shell pwd)/isotmp
APORTS_DIR	?= $(HOME)/aports
REPOS		?= core extra

SUDO		= sudo

# this might need to change...
APKDIRS		?= $(REPOS_DIR)/*/

ISO		?= $(ALPINE_NAME)-$(ALPINE_RELEASE)-$(ALPINE_ARCH).iso
ISO_LINK	?= $(ALPINE_NAME).iso
ISO_DIR		:= $(DESTDIR)/isofs
REPOS_DIR	?= $(HOME)/packages

# limitations for find_apk:
# can not be a subpackage
find_aport 	= $(firstword $(wildcard $(APORTS_DIR)/*/$(1)))
find_repo	= $(subst $(APORTS_DIR),$(REPOS_DIR),$(dir $(call find_aport,$(1))))
find_apk	= $(shell . $(call find_aport,$(1))/APKBUILD ;\
			echo $(call find_repo,$(1))/$(1)-$$pkgver-r$$pkgrel.apk)

KERNEL_FLAVOR	?= grsec
KERNEL_PKGNAME	?= linux-$(KERNEL_FLAVOR)
KERNEL_NAME	:= $(KERNEL_FLAVOR)
KERNEL_APK	:= $(call find_apk,$(KERNEL_PKGNAME))
MODULE_APK	:= $(subst /$(KERNEL_PKGNAME)-,/$(KERNEL_PKGNAME)-mod-,$(KERNEL_APK))

XTABLES_ADDONS_APK:= $(subst xtables-addons,xtables-addons-$(KERNEL_FLAVOR),$(call find_apk,xtables-addons))
DAHDI_LINUX_APK:= $(subst dahdi-linux,dahdi-linux-$(KERNEL_FLAVOR),$(call find_apk,dahdi-linux))
ISCSITARGET_APK:= $(subst iscsitarget,iscsitarget-$(KERNEL_FLAVOR),$(call find_apk,iscsitarget))
MOD_APKS	:= $(MODULE_APK) $(XTABLES_ADDONS_APK) $(DAHDI_LINUX_APK) \
		   $(ISCSITARGET_APK)

KERNEL		:= $(word 3,$(subst -, ,$(notdir $(KERNEL_APK))))-$(word 2,$(subst -, ,$(notdir $(KERNEL_APK))))

ALPINEBASELAYOUT_APK := $(call find_apk,alpine-baselayout)
UCLIBC_APK	:= $(call find_apk,uclibc)
BUSYBOX_APK	:= $(call find_apk,busybox)
APK_TOOLS_APK	:= $(call find_apk,apk-tools)
SYSLINUX_APK	:= $(call find_apk,syslinux)
STRACE_APK	:= $(call find_apk,strace)
ACCT_APK	:= $(call find_apk,acct)

#SOURCE_APKBUILDS := $(wildcard $(addprefix $(APORTS_DIR)/,$(REPOS))/*/APKBUILD)
#SOURCE_APKS	= $(wildcard $(APKDIRS)/*apk)
#APK_BIN		:= $(shell which apk)

#ifneq ($(words $(KERNEL_APK) $(MODULE_APK) $(ALPINEBASELAYOUT_APK) $(UCLIBC_APK) $(BUSYBOX_APK) $(APK_TOOLS_APK)),6)
#$(error I did not find all APKs which I need.)
#endif


all: isofs

help:
	@echo "Alpine ISO builder"
	@echo
	@echo "Type 'make iso' to build $(ISO)"
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
	@echo "ALPINE_NAME:    $(ALPINE_NAME)"
	@echo "ALPINE_RELEASE: $(ALPINE_RELEASE)"
	@echo "KERNEL_FLAVOR:  $(KERNEL_FLAVOR)"
	@echo "APORTS_DIR:     $(APORTS_DIR)"
	@echo "KERNEL:         $(KERNEL)"
	@echo

clean:
	rm -rf $(MODLOOP) $(MODLOOP_DIR) $(MODLOOP_DIRSTAMP) \
		$(INITFS) $(INITFS_DIRSTAMP) $(INITFS_DIR) \
		$(ISO_DIR) $(REPOS_DIRSTAMP) $(ISO_REPOS_DIRSTAMP)

#
# Repos
#
repos: $(REPOS_DIRSTAMP)

REPOS_DIRSTAMP	:= $(DESTDIR)/stamp.repos
$(REPOS_DIRSTAMP): $(SOURCE_APKBUILDS)
	@echo "==> repositories: $(REPOS)"
	@buildrepo -p -a $(APORTS_DIR) -d $(REPOS_DIR) $(REPOS)
	@mkdir -p $(dir $@) && touch $@

%.apk: $(REPOS_DIRSTAMP)

#
# Modloop
#
MODLOOP		:= $(ISO_DIR)/boot/$(KERNEL_NAME).cmg
MODLOOP_DIR	:= $(DESTDIR)/modloop
MODLOOP_DIRSTAMP := $(DESTDIR)/stamp.modloop

modloop: $(MODLOOP)

$(MODLOOP_DIRSTAMP): $(REPOS_DIRSTAMP) $(MOD_APKS)
	@rm -rf $(MODLOOP_DIR)
	@mkdir -p $(MODLOOP_DIR)/lib/modules/
	@for i in $(MOD_APKS); do \
		echo "==> modloop: prepare modules $$i";\
		tar -C $(MODLOOP_DIR) -xzf "$$i"; \
	done
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

INITFS		:= $(ISO_DIR)/boot/$(KERNEL_NAME).gz

INITFS_DIRSTAMP	:= $(DESTDIR)/stamp.initfs
INITFS_DIR	:= $(DESTDIR)/initfs
INITFS_MODDIR	:= $(INITFS_DIR)/lib/modules/$(KERNEL)
INITFS_MODDIRSTAMP := $(DESTDIR)/stamp.initfs.modules

INITFS_APKS	:= $(UCLIBC_APK) $(BUSYBOX_APK) $(ACCT_APK) 
INITFS_RAWBASEFILES  := etc/mdev.conf etc/passwd etc/group etc/fstab \
			etc/modules etc/modprobe.d/blacklist lib/mdev
INITFS_BASEFILES     := $(addprefix $(INITFS_DIR)/, $(INITFS_RAWBASEFILES))

initfs: $(INITFS)

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
	@touch $(INITFS_DIRSTAMP)

$(INITFS_BASEFILES): $(INITFS_DIRSTAMP) $(ALPINEBASELAYOUT_APK)
	@echo "==> initramfs: $(notdir $(ALPINEBASELAYOUT_APK))"
	@tar -C $(INITFS_DIR) -xzf $(ALPINEBASELAYOUT_APK) $(INITFS_RAWBASEFILES)
	#@echo "floppy" >> "$(INITFS_DIR)/etc/modules"
	#@echo "libata dma=0" >> "$(INITFS_DIR)/etc/modules"
	@touch $(INITFS_BASEFILES)

$(INITFS_DIR)/init: initramfs-init $(INITFS_DIRSTAMP)
	@echo "==> initramfs: init script"
	@cp initramfs-init "$(INITFS_DIR)/init"

$(INITFS_DIR)/sbin/bootchartd: bootchartd $(INITFS_DIRSTAMP)
	@echo "==> initramfs: bootchartd"
	@cp bootchartd "$(INITFS_DIR)/sbin/bootchartd"

ifeq ($(APK_BIN),)
$(INITFS_DIR)/sbin/apk: $(APK_TOOLS_APK) $(INITFS_DIRSTAMP)
	@echo "==> initramfs: $(notdir $(APK_TOOLS_APK))"
	@tar -C $(INITFS_DIR) -xzf $(APK_TOOLS_APK) sbin/apk
	@touch $@
else
$(INITFS_DIR)/sbin/apk: $(APK_BIN) $(INITFS_DIRSTAMP)
	@echo "==> initramfs: copy $(APK_BIN) from buildroot"
	@cp $(APK_BIN) "$(INITFS_DIR)/sbin"
endif

$(INITFS_MODDIRSTAMP): $(INITFS_DIRSTAMP) $(INITFS_MODFILES) $(MODLOOP_DIRSTAMP)
	@echo "==> initramfs: $(notdir $(MODULE_APK))"
	@rm -rf $(INITFS_DIR)/lib/modules
	@mkdir -p $(addprefix $(INITFS_MODDIR)/kernel/,drivers fs)
	@for i in acpi ata block ide scsi cdrom usb message hid md; do \
		cp -flLpR $(MODLOOP_DIR)/lib/modules/*/kernel/drivers/$$i \
			$(INITFS_MODDIR)/kernel/drivers/ ; \
	done
	@for i in isofs vfat fat nls jbd 'ext*' cramfs '*.ko'; do \
		cp -flLpR $(MODLOOP_DIR)/lib/modules/*/kernel/fs/$$i \
			$(INITFS_MODDIR)/kernel/fs/ 2>/dev/null; \
	done
	@cp -flLpR $(MODLOOP_DIR)/lib/modules/*/kernel/lib \
		$(INITFS_MODDIR)/kernel/
	@depmod $(KERNEL) -b $(INITFS_DIR)
	@touch $(INITFS_MODDIRSTAMP)

$(INITFS): $(INITFS_DIRSTAMP) $(INITFS_DIR)/init $(INITFS_DIR)/sbin/bootchartd $(INITFS_DIR)/sbin/apk $(INITFS_MODDIRSTAMP) $(INITFS_BASEFILES)
	@echo "==> initramfs: creating $(notdir $(INITFS))"
	@mkdir -p $(dir $(INITFS))
	@(cd $(INITFS_DIR) && find . | cpio -o -H newc | gzip -9) > $(INITFS)

#
# Vserver template rules
#
VSTEMPLATE	:= $(ISO_DIR)/vs-template.tar.bz2
VSTEMPLATE_DIR 	:= $(DESTDIR)/vs-template

vstemplate: $(VSTEMPLATE)
	@echo "==> vstemplate: built $(VSTEMPLATE)"

$(VSTEMPLATE):
	@$(SUDO) rm -rf "$(VSTEMPLATE_DIR)"
	@$(SUDO) mkdir -p "$(VSTEMPLATE_DIR)"
	@$(SUDO) apk add --initdb --root $(VSTEMPLATE_DIR) alpine-baselayout
	@cd $(VSTEMPLATE_DIR) && $(SUDO) tar -jcf $@ *

#
# ISO rules
#

ISOLINUX	:= $(ISO_DIR)/isolinux
ISOLINUX_BIN	:= $(ISOLINUX)/isolinux.bin
ISOLINUX_CFG	:= $(ISOLINUX)/isolinux.cfg
SYSLINUX_CFG	:= $(ISO_DIR)/syslinux.cfg

$(ISOLINUX_BIN): $(SYSLINUX_APK)
	@echo "==> iso: install isolinux"
	@mkdir -p $(dir $(ISOLINUX_BIN))
	@tar -O -zxf $(SYSLINUX_APK) usr/share/syslinux/isolinux.bin > $@
#	@cp /usr/share/syslinux/isolinux.bin $(ISOLINUX_BIN)

$(ISOLINUX_CFG):
	@echo "==> iso: configure isolinux"
	@mkdir -p $(dir $(ISOLINUX_BIN))
	@echo "timeout 20" >$(ISOLINUX_CFG)
	@echo "prompt 1" >>$(ISOLINUX_CFG)
	@echo "default $(KERNEL_NAME)" >>$(ISOLINUX_CFG)
	@echo "label $(KERNEL_NAME)" >>$(ISOLINUX_CFG)
	@echo "	kernel /boot/$(KERNEL_NAME)" >>$(ISOLINUX_CFG)
	@echo "	append initrd=/boot/$(KERNEL_NAME).gz alpine_dev=cdrom:iso9660 modules=sd-mod,usb-storage,floppy quiet" >>$(ISOLINUX_CFG)

$(SYSLINUX_CFG):
	@echo "==> iso: configure syslinux"
	@echo "timeout 20" >$@
	@echo "prompt 1" >>$@
	@echo "default $(KERNEL_NAME)" >>$@
	@echo "label $(KERNEL_NAME)" >>$@
	@echo "	kernel /boot/$(KERNEL_NAME)" >>$@
	@echo "	append initrd=/boot/$(KERNEL_NAME).gz alpine_dev=sda1:vfat modules=sd-mod,usb-storage quiet" >>$@

ISO_KERNEL	:= $(ISO_DIR)/boot/$(KERNEL_NAME)
ISO_PKGDIR	:= $(ISO_DIR)/packages
ISO_REPOS	:= $(addprefix $(ISO_PKGDIR)/,$(REPOS))
ISO_APKINDEX	:= $(addsuffix /APK_INDEX.gz,$(ISO_REPOS))
ISO_REPOS_DIRSTAMP := $(DESTDIR)/stamp.isorepos
ISOFS_DIRSTAMP	:= $(DESTDIR)/stamp.isofs

$(ISO_REPOS_DIRSTAMP): $(addsuffix /APK_INDEX.gz,$(addprefix $(REPOS_DIR)/,$(REPOS)))
	@echo "==> iso: prepare repositories $(REPOS)"
	@rm -rf $(ISO_PKGDIR)
	@mkdir -p $(ISO_REPOS)
	@for r in $(REPOS); do \
		for a in $(REPOS_DIR)/$$r/*; do \
			ln -f "$$a" $(ISO_PKGDIR)/$$r/$${a##*/} 2>/dev/null \
			|| cp -r "$$a" $(ISO_PKGDIR)/$$r/$${a##*/} ;\
		done;\
	done
	@touch $@

#$(ISO_APKINDEX): $(ISO_REPOS)
#	@apk index $(dir $@)/*.apk | gzip -9 > $@

$(ISO_KERNEL): $(KERNEL_APK)
	@echo "==> iso: install kernel $(KERNEL)"
	@mkdir -p $(dir $(ISO_KERNEL))
	@tar -C $(ISO_DIR) -xzf $(KERNEL_APK)
	@rm -f $(ISO_DIR)/.[A-Z]*
	@touch $(ISO_KERNEL)

$(ISOFS_DIRSTAMP): $(MODLOOP) $(INITFS) $(ISOLINUX_CFG) $(ISOLINUX_BIN) $(ISO_KERNEL) $(ISO_REPOS_DIRSTAMP) $(SYSLINUX_CFG)
	@touch $@

$(ISO): $(ISOFS_DIRSTAMP)
	@echo "==> iso: building $(notdir $(ISO))"
	@echo "$(ALPINE_NAME)-$(ALPINE_RELEASE) $(BUILD_DATE)" \
		> $(ISO_DIR)/.alpine-release
	@genisoimage -o $(ISO) -l -J -R \
		-b isolinux/isolinux.bin \
		-c isolinux/boot.cat	\
		-no-emul-boot		\
		-boot-load-size 4	\
		-boot-info-table	\
		-quiet			\
		$(ISO_OPTS)		\
		$(ISO_DIR)
	@ln -fs $@ $(ISO_LINK)

isofs: $(ISOFS_DIRSTAMP)
iso: $(ISO)

#
# SHA1 sum of ISO
#
SHA1	:= $(ISO).sha1

$(SHA1):	$(ISO)
	@echo "==> Generating sha1 sum"
	@sha1sum $(ISO) > $@ || rm -f $@

sha1: $(SHA1)
