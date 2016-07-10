#!/usr/bin/ruby
# Copied from the archlinux android-tools PKGBUILD.

# Android build system is complicated and does not allow to build
# separate parts easily.
# This script tries to mimic Android build rules.

def expand(dir, files)
  files.map{|f| File.join(dir,f)}
end

# Compiles sources to *.o files.
# Returns array of output *.o filenames
def compile(sources, cflags)
  outputs = []
  for s in sources
    ext = File.extname(s)

    case ext
    when '.c'
        cc = 'gcc'
        lang_flags = '-std=gnu11 $CFLAGS $CPPFLAGS'
    when '.cpp', '.cc'
        cc = 'g++'
        lang_flags = '-std=gnu++11 $CXXFLAGS $CPPFLAGS'
    else
        raise "Unknown extension #{ext}"
    end

    output = s + '.o'
    outputs << output
    puts "#{cc} -o #{output} #{lang_flags} #{cflags} -c #{s}\n"
  end

  return outputs
end

# Links object files
def link(output, objects, ldflags)
  puts "g++ -o #{output} #{objects.join(' ')} #{ldflags} $LDFLAGS"
end

minicryptfiles = %w(
  dsa_sig.c
  p256_ec.c
  rsa.c
  sha.c
  p256.c
  p256_ecdsa.c
  sha256.c
)
libminicrypt = compile(expand('core/libmincrypt', minicryptfiles), '-Icore/include')
libmkbootimg = compile(['core/mkbootimg/mkbootimg.c'], '-Icore/include')
link('mkbootimg', libminicrypt + libmkbootimg, nil)


adbdfiles = %w(
  adb.cpp
  adb_auth.cpp
  adb_io.cpp
  adb_listeners.cpp
  adb_utils.cpp
  sockets.cpp
  transport.cpp
  transport_local.cpp
  transport_usb.cpp

  fdevent.cpp
  get_my_path_linux.cpp
  usb_linux.cpp

  adb_auth_host.cpp
)
libadbd = compile(expand('core/adb', adbdfiles), '-DADB_HOST=1 -fpermissive -Icore/include -Icore/base/include')

adbfiles = %w(
  adb_main.cpp
  console.cpp
  commandline.cpp
  adb_client.cpp
  services.cpp
  file_sync_client.cpp
)
libadb = compile(expand('core/adb', adbfiles), '-DADB_REVISION=\"$PKGVER\" -D_GNU_SOURCE -DADB_HOST=1 -DWORKAROUND_BUG6558362 -fpermissive -Icore/include -Icore/base/include')

basefiles = %w(
  file.cpp
  stringprintf.cpp
  strings.cpp
)
libbase = compile(expand('core/base', basefiles), '-DADB_HOST=1 -Icore/base/include -Icore/include')

logfiles = %w(
  logd_write.c
  log_event_write.c
  fake_log_device.c
)
liblog = compile(expand('core/liblog', logfiles), '-DLIBLOG_LOG_TAG=1005 -DFAKE_LOG_DEVICE=1 -D_GNU_SOURCE -Icore/log/include -Icore/include')

cutilsfiles = %w(
  load_file.c
  socket_inaddr_any_server.c
  socket_local_client.c
  socket_local_server.c
  socket_loopback_client.c
  socket_loopback_server.c
  socket_network_client.c
)
libcutils = compile(expand('core/libcutils', cutilsfiles), '-D_GNU_SOURCE -Icore/include')

link('adb', libbase + liblog + libcutils + libadbd + libadb, '-lrt -ldl -lpthread -lssl -lcrypto')


fastbootfiles = %w(
  protocol.c
  engine.c
  bootimg_utils.cpp
  fastboot.cpp
  util.c
  fs.c
  usb_linux.c
  util_linux.c
)
libfastboot = compile(expand('core/fastboot', fastbootfiles), '-DFASTBOOT_REVISION=\"$PKGVER\" -D_GNU_SOURCE -Icore/include -Icore/libsparse/include -Icore/mkbootimg -Iextras/ext4_utils -Iextras/f2fs_utils')

sparsefiles = %w(
  backed_block.c
  output_file.c
  sparse.c
  sparse_crc32.c
  sparse_err.c
  sparse_read.c
)
libsparse = compile(expand('core/libsparse', sparsefiles), '-Icore/libsparse/include')

zipfiles = %w(
  zip_archive.cc
)
libzip = compile(expand('core/libziparchive', zipfiles), '-Icore/base/include -Icore/include')

utilfiles = %w(
  FileMap.cpp
)
libutil = compile(expand('core/libutils', utilfiles), '-Icore/include')

ext4files = %w(
  make_ext4fs.c
  ext4fixup.c
  ext4_utils.c
  allocate.c
  contents.c
  extent.c
  indirect.c
  sha1.c
  wipe.c
  crc16.c
  ext4_sb.c
)
libext4 = compile(expand('extras/ext4_utils', ext4files), '-Icore/libsparse/include -Icore/include -Ilibselinux/include')

selinuxfiles = %w(
  src/callbacks.c
  src/check_context.c
  src/freecon.c
  src/init.c
  src/label.c
  src/label_file.c
  src/label_android_property.c
)
libselinux = compile(expand('libselinux', selinuxfiles), '-DAUDITD_LOG_TAG=1003 -D_GNU_SOURCE -DHOST -Ilibselinux/include')

link('fastboot', libsparse + libzip + liblog + libutil + libbase + libext4 + libselinux + libfastboot, '-lz -lpcre')
