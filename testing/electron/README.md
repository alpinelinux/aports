# electron

This is the `electron` package for Alpine Linux.

Please report any issues [using Gitlab](https://gitlab.alpinelinux.org/alpine/aports/-/issues/new) and tag @ayakael

## Building electron

Electron is an application framework based on `chromium`. Just like `chromium`,
and any Google application, the build process is a form of [hostile
architecture] (https://en.wikipedia.org/wiki/Hostile_architecture) It's quite
literally chromium with patches applied on top for the most part. The build
process applies a series of git patches against `chromium` from directories
with a script.

Its source code isn't available as a downloadable tarball. It is only fetchable
using Google's `gclient` available in `depot_tools` with a reimplemented
version in the `teapot` package. By executing, `abuild snapshot`, the tarball
can be fetched and packaged, as long as `gclient` is in your path. For ease of
maintenance, a workflow on [Ayakael's Forge](https://ayakael.net/mirrors/electron)
automatically fetches and packages the source code on new releases and makes it
available in a [generic Forgejo repository](https://ayakael.net/mirrors/-/packages/generic/electron).

## Electron maintenance cycle

Security / bug fixes land from upstream land randomly, but chromium security fixes land
basically weekly around Tuesday in `America/Los_Angeles`. Minor relases only require
an upgrade to the `electron` packages. It is advisable to follow chromium weekly
security fixes, although following `electron` minor releases is fine.

Major version upgrades require a more thorough approach. For one, most changes
can be backported from `chromium` APKBUILD by diffing the previous version
packaged with `electron` with the current (set with `_chromium` var). You also
need to rebuild all `electron` apps, with patches sometimes necessary when
upstream bumps to a new `nodejs` major verion. Major electron releases are
every two `chromium` major releases, with [dates known well ahead]
(https://chromiumdash.appspot.com/schedule) with a few major releases of
`electron` [officially supported at a time](https://www.electronjs.org/docs/latest/tutorial/electron-timelines).

Steps, in a nutshell:

1. Set `pkgver` to up-to-date version

2. Optional: fetch source-code using `abuild snapshot`, making sure `gclient`
is in your path

3. Update source checksum using `abuild checksum`

4. If major update, backport changes from `chromium` aport and bump `pkgrel`
for all electron-based applications.

## Why is this package still in testing

[Work is under way](https://gitlab.alpinelinux.org/alpine/aports/-/issues/15760)
to make this aport ready for `community`

Until that happens, this package is also kept-to-date against the latest
release of Alpine Linux in [Ayakael's Forge](https://ayakael.net/forge/-/packages/alpine/signal-desktop)
This is true of all Ayakael's packages still in `testing`.
