# signal-desktop

This is the `signal-desktop` package for Alpine Linux.

Please report any issues [using Gitlab](https://gitlab.alpinelinux.org/alpine/aports/-/issues/new) and tag @ayakael

## Building signal-desktop

Signal-desktop is an electron application that is rather complex to build

The first layer of complexity is the use of dependencies that are themselves
rather complex to build. Some are based on nodejs, others rust. Those
dependencies are built before signal-desktop, like ringrtc, webrtc and
libsignal. The versions of those dependencies are tracked in different files,
which adds complexity when maintaining the package. Executing `abuild
_update_depends` automatically fetches the expected versions and updates
the relevant variables. 

## Updating signal-desktop

In a nutshell:

1. Set `pkgver` to up-to-date version

2. Update the dependency versions using  `abuild _update_depends`

3. Optional: fetch webrtc using `abuild snapshot`, making sure `client`
is in your path

4. Update source checksum using `abuild checksum`

## Finding dependency version information

Here is where the version information is stored. It is different for every
extra dependency.

* _libsignalver: follow signal-desktop package.json ->
@signalapp/libsignal-client
* _ringrtcver: follow signal-desktop package.json -> @signalapp/ringrtc
* _webrtcver: follow ringrtc (on version above) -> config/version.properties ->
webrtc.version downloading tarball generated with abuild snapshot (with gclient
dependencies fetched)
* _stokenizerver: follow @signalapp/better-sqlite3 (on version in package.json)
-> deps/download.js -> TOKENIZER_VERSION

## Why is this package still in testing

As `electron` is still in testing, this package cannot yet be moved to
`community`. Until that changes, this package is also kept-to-date against the
latest release of Alpine Linux (along with `electron`) in
[Ayakael's Forge](https://ayakael.net/forge/-/packages/alpine/signal-desktop).
This is true of all Ayakael's packages still in `testing`.
