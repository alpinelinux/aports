# zotero

This is the `zotero` package for Alpine Linux.

Please report any issues [using Gitlab](https://gitlab.alpinelinux.org/alpine/aports/-/issues/new) and tag @ayakael

## Zotero's build process

Zotero was originally a Firefox extension. When Firefox abandonned xul-based
extensions, the Zotero dev team created a Zotero standalone app based on the
Firefox codebase. Rather than fork Firefox source code, they designed a build
system that would download Firefox as a prebuilt tar and inject Zotero code
into it. Since we cannot package prebuilt bits, this aport builds the version
of Firefox Zotero expects, forked from the `firefox-esr` aport, and builds
Zotero on top of it. As you can imagine, this build system is anything but
standard. At the very least, this aports' approach ensures that everything is
built from source, allowing Zotero to be used on Alpine

Zotero's git repo has many submodules, which means that the tarball available
in GitHub does not contain all the source code needed to build it. Thus, a
workflow on [Ayakael's Forge](https://ayakael.net/mirrors/zotero) automatically
packages a buildable tarball on every new release, and  makes it available as a
[generic Forgejo package](https://ayakael.net/mirrors/-/packages/generic/zotero)
