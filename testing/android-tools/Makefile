all: mkbootimg fastboot adb

install:
	install -m755 -d $(DESTDIR)/usr/bin
	install -m755 -t $(DESTDIR)/usr/bin mkbootimg fastboot adb

clean:
	rm -f **/*.o

.PHONY: clean




MKBOOTIMG_SRCS += $(wildcard core/libmincrypt/*.c)
MKBOOTIMG_SRCS += core/mkbootimg/mkbootimg.c

MKBOOTIMG_CFLAGS += -Icore/include

mkbootimg: $(MKBOOTIMG_SRCS)
	$(CC) -o $@ $(CFLAGS) $(MKBOOTIMG_CFLAGS) $(LDFLAGS) $(MKBOOTIMG_LIBS) $(MKBOOTIMG_SRCS)




ADB_SRCS += core/adb/adb.c
ADB_SRCS += core/adb/adb_auth_host.c
ADB_SRCS += core/adb/adb_client.c
ADB_SRCS += core/adb/commandline.c
ADB_SRCS += core/adb/console.c
ADB_SRCS += core/adb/fdevent.c
ADB_SRCS += core/adb/file_sync_client.c
ADB_SRCS += core/adb/get_my_path_linux.c
ADB_SRCS += core/adb/services.c
ADB_SRCS += core/adb/sockets.c
ADB_SRCS += core/adb/transport.c
ADB_SRCS += core/adb/transport_local.c
ADB_SRCS += core/adb/transport_usb.c
ADB_SRCS += core/adb/usb_linux.c
ADB_SRCS += core/adb/usb_vendors.c
ADB_SRCS += core/libcutils/load_file.c
ADB_SRCS += core/libcutils/socket_inaddr_any_server.c
ADB_SRCS += core/libcutils/socket_local_client.c
ADB_SRCS += core/libcutils/socket_local_server.c
ADB_SRCS += core/libcutils/socket_loopback_client.c
ADB_SRCS += core/libcutils/socket_loopback_server.c
ADB_SRCS += core/libcutils/socket_network_client.c
ADB_SRCS += core/libzipfile/centraldir.c
ADB_SRCS += core/libzipfile/zipfile.c

ADB_CFLAGS  += -DADB_HOST=1 -DHAVE_FORKEXEC=1 -DHAVE_OFF64_T=1 -DHAVE_TERMIO_H -I core/include -I core/adb
ADB_LIBS += -lcrypto -lpthread -lz

adb: $(ADB_SRCS)
	$(CC) -o $@ $(CFLAGS) $(ADB_CFLAGS) $(LDFLAGS) $(ADB_SRCS) $(ADB_LIBS)



FASTBOOT_SRCS += core/fastboot/bootimg.c
FASTBOOT_SRCS += core/fastboot/engine.c
FASTBOOT_SRCS += core/fastboot/fastboot.c
FASTBOOT_SRCS += core/fastboot/protocol.c
FASTBOOT_SRCS += core/fastboot/usb_linux.c
FASTBOOT_SRCS += core/fastboot/util_linux.c
FASTBOOT_SRCS += core/fastboot/util.c
FASTBOOT_SRCS += core/fastboot/fs.c
FASTBOOT_SRCS += core/libsparse/backed_block.c
FASTBOOT_SRCS += core/libsparse/output_file.c
FASTBOOT_SRCS += core/libsparse/sparse.c
FASTBOOT_SRCS += core/libsparse/sparse_crc32.c
FASTBOOT_SRCS += core/libsparse/sparse_err.c
FASTBOOT_SRCS += core/libsparse/sparse_read.c
FASTBOOT_SRCS += core/libzipfile/centraldir.c
FASTBOOT_SRCS += core/libzipfile/zipfile.c
FASTBOOT_SRCS += extras/ext4_utils/allocate.c
FASTBOOT_SRCS += extras/ext4_utils/contents.c
FASTBOOT_SRCS += extras/ext4_utils/crc16.c
FASTBOOT_SRCS += extras/ext4_utils/ext4_utils.c
FASTBOOT_SRCS += extras/ext4_utils/ext4_sb.c
FASTBOOT_SRCS += extras/ext4_utils/extent.c
FASTBOOT_SRCS += extras/ext4_utils/indirect.c
FASTBOOT_SRCS += extras/ext4_utils/make_ext4fs.c
FASTBOOT_SRCS += extras/ext4_utils/sha1.c
FASTBOOT_SRCS += extras/ext4_utils/uuid.c
FASTBOOT_SRCS += extras/ext4_utils/wipe.c
FASTBOOT_SRCS += extras/f2fs_utils/f2fs_utils.c
FASTBOOT_SRCS += extras/f2fs_utils/f2fs_dlutils.c
FASTBOOT_SRCS += extras/f2fs_utils/f2fs_ioutils.c
FASTBOOT_SRCS += libselinux/src/callbacks.c
FASTBOOT_SRCS += libselinux/src/check_context.c
FASTBOOT_SRCS += libselinux/src/freecon.c
FASTBOOT_SRCS += libselinux/src/init.c
FASTBOOT_SRCS += libselinux/src/label.c
FASTBOOT_SRCS += libselinux/src/label_android_property.c
FASTBOOT_SRCS += libselinux/src/label_file.c

FASTBOOT_CFLAGS  += -DHAVE_OFF64_T=1 -std=gnu99 -I core/mkbootimg -I core/libsparse/include -I core/include -I extras/ext4_utils -I extras/f2fs_utils -I libselinux/include -I f2fs-tools/include -I f2fs-tools/mkfs
FASTBOOT_LIBS += -lz -ldl -lpcre

fastboot: $(FASTBOOT_SRCS)
	$(CC) -o $@ $(CFLAGS) $(FASTBOOT_CFLAGS) $(LDFLAGS) $(FASTBOOT_SRCS) $(FASTBOOT_LIBS)
