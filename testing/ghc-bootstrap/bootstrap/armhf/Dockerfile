# Mostly self contained setup to build a stage2 ghc for musl
from debian:8.0

# Install stock bindist for cross compile
env ghc 7.10.3
env arch x86_64
env llvm 3.7.1
env cabal 1.22.9.0

# all needed packages for compiling
run apt-get clean && \
    apt-get update && \
    apt-get install -y \
      binutils-gold \
      musl-tools \
      build-essential \
      wget \
      curl \
      libncurses-dev \
      autoconf \
      elfutils \
      libgmp-dev \
      zlib1g-dev \
      git \
      libtool \
      pkg-config \
      libffi-dev \
      cmake \
      g++ \
      python \
      pixz \
      openssl \
      bison \
      flex \
      git

add http://llvm.org/releases/$llvm/llvm-$llvm.src.tar.xz /tmp/
add http://llvm.org/releases/$llvm/polly-$llvm.src.tar.xz /tmp/

# Install a non ancient version of llvm on debian, I'm purposefully ignoring
# debian repos in favor of compiling to not have to deal with
# "what debian upstream has a current version of llvm" nonsense, takes more
# time to do that than just build the right llvm from source.
workdir /tmp
copy bootstrap/llvm-$llvm.sh /tmp/llvm.sh
run openssl sha1 llvm-$llvm.src.tar.xz | grep "SHA1(llvm-3.7.1.src.tar.xz)= 5dbdcafac105273dcbff94c68837a66c6dd78cef" && \
    openssl sha1 polly-$llvm.src.tar.xz | grep "SHA1(polly-3.7.1.src.tar.xz)= 0e3a461907cde7505fbdb44bf61ff318aa9254f7" && \
    tar xJpf /tmp/llvm-$llvm.src.tar.xz && \
    tar xJpf /tmp/polly-$llvm.src.tar.xz && \
    /tmp/llvm.sh && \
    rm -fr /tmp/llvm-$llvm.src

add https://downloads.haskell.org/~ghc/$ghc/ghc-$ghc-$arch-deb8-linux.tar.xz /tmp/

# Install debian ghc binary from upstream.
workdir /tmp
run openssl sha1 ghc-$ghc-$arch-deb8-linux.tar.xz | grep "SHA1(ghc-7.10.3-x86_64-deb8-linux.tar.xz)= bab16f95ef4fe6b7cc2fb6b36a02dceeeb53faa4" && \
    tar xJpf /tmp/ghc-$ghc-$arch-deb8-linux.tar.xz
workdir /tmp/ghc-$ghc
run ./configure --prefix=/usr && \
    make -j1 install && \
    rm -fr /tmp/ghc-$ghc

add https://www.haskell.org/cabal/release/cabal-install-$cabal/cabal-install-$cabal.tar.gz /tmp/

# Install cabal so we can install alex/happy to pull off of git
# bootstrap cabal and install alex/happy the same way apks are built
# only globally
workdir /tmp
run openssl sha1 cabal-install-$cabal.tar.gz | grep "SHA1(cabal-install-1.22.9.0.tar.gz)= f1375c928794f45f253b8ec92c2af4732fec597b" && \
    tar xzpf /tmp/cabal-install-$cabal.tar.gz
workdir /tmp/cabal-install-$cabal
run ./bootstrap.sh --global --no-doc && \
    cabal update && \
    cabal install --global alex happy && \
    rm -fr /tmp/cabal-install-$cabal

# First up, install/compile the cross compiler with musl libc
# armv7 hard float cross compiler, we basically rebuild ghc again here with
# the cross compiler, and the llvm we built for x86_64 as well
workdir /tmp
run git clone --depth 1 https://github.com/GregorR/musl-cross.git musl-cross
workdir /tmp/musl-cross
run echo GCC_BUILTIN_PREREQS=yes >> config.sh && \
    echo ARCH=arm >> config.sh && \
    echo TRIPLE=arm-linux-musleabihf >> config.sh && \
    echo GCC_BOOTSTRAP_CONFFLAGS=\"--with-arch=armv6 --with-float=hard --with-fpu=vfp\" >> config.sh && \
    echo GCC_CONFFLAGS=\"--with-arch=armv6 --with-float=hard --with-fpu=vfp\" >> config.sh && \
    echo GCC_STAGE1_NOOPT=1 >> config.sh && \
    echo CC_BASE_PREFIX=/usr >> config.sh && \
    echo MAKEFLAGS=-j$(grep -c processor /proc/cpuinfo) >> config.sh && \
    echo "BINUTILS_CONFFLAGS='CXXFLAGS=-fpermissive --enable-gold --enable-plugins --disable-werror'" >> config.sh && \
    echo "CFLAGS='-g -O2 -fPIC -DPIC'" >> config.sh && \
    echo "CPPFLAGS='-fPIC -DPIC'" >> config.sh && \
    echo "LDFLAGS='-fPIC -DPIC'" >> config.sh
copy bootstrap/gmpurl.patch gmpurl.patch
run patch -p1 < gmpurl.patch && \
    ./build.sh && \
    rm -fr /tmp/musl-cross

add http://downloads.haskell.org/~ghc/8.0.1/ghc-8.0.1-src.tar.xz /tmp/

env tardir /tmp/root
env destdir /tmp/root/armhf
env crosscc arm-linux-musleabihf-gcc
env target arm-linux-musleabihf
env triple arm-unknown-linux-musleabihf
env ghc 8.0.1  

# add cross toolchain to PATH
env PATH /usr/$target/bin:$PATH

workdir /tmp
run openssl sha1 ghc-$ghc-src.tar.xz | grep "SHA1(ghc-8.0.1-src.tar.xz)= 585a2d34a17ce2452273147f2e3cef1a2efe1aa5" && \
    tar xJpf /tmp/ghc-$ghc-src.tar.xz
workdir /tmp/ghc-$ghc
env PATH $PATH:/usr/$triple/bin  
copy bootstrap/$arch/bootstrap.patch bootstrap.patch  
run patch -p1 < bootstrap.patch  
run cp mk/build.mk.sample mk/build.mk && \  
    ./boot && \  
    echo "BuildFlavour         = quick-llvm" >> mk/build.mk && \  
    echo "INTEGER_LIBRARY      = integer-simple" >> mk/build.mk && \  
    echo "HADDOCK_DOCS         = NO" >> mk/build.mk && \  
    echo "BUILD_SPHINX_HTML    = NO" >> mk/build.mk && \  
    echo "BUILD_SPHINX_PS      = NO" >> mk/build.mk && \  
    echo "BUILD_SPHINX_PDF     = NO" >> mk/build.mk && \  
    ./configure \  
  --target=$target \  
  --prefix=/usr  
run make -j$(grep -c processor /proc/cpuinfo) || make -j1  
run make -j1 install STRIP_CMD=$target-strip DESTDIR=$destdir
  
# unlit and hp2ps both build using the stage0, not having luck
# getting the build patched right so for now lets just
# remove and rebuild these c helper programs
run rm $(find $destdir -name "*-hp2ps")

# remove target prefix from stage2 binaries
# HACK, just build unlit with the cross compiler and move it to /usr/bin in the install dir
workdir /tmp/ghc-$ghc/utils/unlit
run $crosscc unlit.c -o $(find $destdir -name unlit)

# remove target prefix from stage2 binaries
workdir $destdir/usr/bin
run (for i in $triple-* ; do ln -s $i ${i#$triple-} || /bin/true ; done )
copy bootstrap/armhf/settings /tmp/settings
run mv /tmp/settings $(find $destdir -name settings -type f)
run rm -fr $destdir/usr/share/doc

workdir $tardir
# Compress to xz via pixz because xz is normally too
# old for -TN multithreads
run tar -I'pixz -9' -cf /tmp/ghc-$ghc-$triple.tar.xz .
