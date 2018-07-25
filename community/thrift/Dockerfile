FROM alpine:latest

ENV UNAME alpine
ENV THRIFT_VER 0.11.0
ENV LUA_VER 5.1
ENV PKG_REV 0

RUN apk update && apk add alpine-sdk git
RUN apk add autoconf automake bison flex libtool pkgconfig
RUN apk add apache-ant maven boost glib go libevent lua${LUA_VER} nodejs openssl openjdk8-jre-base openjdk8-jre-lib openjdk8 perl-bit-vector perl-class-accessor python2 python3 ruby rust zlib boost-dev glib-dev libevent-dev lua${LUA_VER}-dev openssl-dev python2-dev python3-dev zlib-dev

RUN adduser -D ${UNAME}
RUN echo "alpine ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${UNAME}

RUN addgroup alpine abuild
RUN mkdir -p /var/cache/distfiles
RUN chmod a+w /var/cache/distfiles
RUN chgrp abuild /var/cache/distfiles
RUN chmod g+w /var/cache/distfiles
COPY abuild.conf /etc/abuild.conf

USER ${UNAME}
WORKDIR /home/${UNAME}
RUN git config --global user.name "Alpine Linux"
RUN git config --global user.email "invalid@alpinelinux.org"
RUN git clone https://github.com/alpinelinux/aports
RUN abuild-keygen -a -i

#
# XXX: @CF: 20180709: need to patch musl implementation of getaddrinfo to support AI_ADDRCONFIG
#
WORKDIR /home/${UNAME}/aports/main/musl
COPY musl-1.1.19-getaddrinfo-ai-addrconfig.patch 3000-getaddrinfo-ai-addrconfig.patch
RUN sed -e 's|\(2000-pthread.*\)|\1\n3000-getaddrinfo-ai-addrconfig.patch|g' < APKBUILD > APKBUILD.new
RUN mv APKBUILD.new APKBUILD 
RUN abuild checksum
RUN abuild -r
WORKDIR /home/${UNAME}/packages/main/x86_64
RUN sudo apk add musl-1.1.19-r10.apk musl-dev-1.1.19-r10.apk musl-utils-1.1.19-r10.apk libc6-compat-1.1.19-r10.apk

#RUN newapkbuild \
# -n thrift \
# -d "Thrift is a lightweight, language-independent software stack with an associated code generation mechanism for RPC" \
# -u 'https://github.com/apache/thrift/archive/${THRIFT_VER}.tar.gz' \
# -l Apache-2.0 \
# -a \
# https://github.com/apache/thrift/archive/${THRIFT_VER}.tar.gz
WORKDIR /home/${UNAME}
RUN mkdir -p thrift/src
COPY APKBUILD thrift
WORKDIR /home/${UNAME}/thrift
COPY lib-lua-makefile-am-longnumber.patch .
COPY lib-perl-makefile-am-prefix.patch .
COPY lib-cpp-makefile-am-libadd-zlib-libevent.patch .
COPY configure-ac-boost-thread-mt.patch .
COPY lib-cpp-test-TNonblockingSSLServerTest-cpp-signal-h.patch .
COPY lib-cpp-test-Makefile-am-TEST-ENVIRONMENT.patch .
COPY configure-ac-lua-${LUA_VER}.patch .
COPY lib-cpp-mutex-adaptive-errorcheck-recursive-init.patch .
COPY lib-c_glib-test-testdebugproto-musl-broken-neginf.patch .
COPY lib-c_glib-test-testtransportsslsocket-musl-different-string-for-unknown-host.patch .
COPY lib-py-src-ext-endian-h-use-system-endian-h.patch .
COPY test-py-ignore-poorly-written-python-triggers-eaddrinuse.patch .
RUN abuild checksum
RUN abuild -r

WORKDIR /home/${UNAME}/packages/alpine/x86_64
RUN sudo apk add libthrift-${THRIFT_VER}-r${PKG_REV}.apk
RUN sudo apk add thrift-${THRIFT_VER}-r${PKG_REV}.apk
RUN sudo apk add libthrift-dev-${THRIFT_VER}-r${PKG_REV}.apk
RUN sudo apk add libthriftc-${THRIFT_VER}-r${PKG_REV}.apk
RUN sudo apk add libthriftc-dev-${THRIFT_VER}-r${PKG_REV}.apk
RUN sudo apk add lua-thrift-${THRIFT_VER}-r${PKG_REV}.apk
RUN sudo apk add perl-thrift-${THRIFT_VER}-r${PKG_REV}.apk
RUN sudo apk add py-thrift-${THRIFT_VER}-r${PKG_REV}.apk
# XXX: @CF: 20180715: users should probably just stick with "npm install thrift"
# RUN sudo apk add node-thrift-${THRIFT_VER}-r${PKG_REV}.apk

WORKDIR /home/${UNAME}

