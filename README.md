Alpine Linux aports repository
==============================

This repository contains the APKBUILD files for each and every
Alpine Linux package, along with the required patches and scripts,
if any.

It also contains some extra files and directories related to testing
(and therefore, building) those packages on GitLab (via GitLab CI).

If you want to contribute, please read the
[contributor guide](https://wiki.alpinelinux.org/wiki/Alpine_Linux:Contribute)
and feel free to either submit a git patch on the Alpine aports
mailing list (~alpine/aports@lists.alpinelinux.org), or to submit a
merge request on [GitLab](https://gitlab.alpinelinux.org/alpine/aports).


Git Hooks
---------

You can find some useful git hooks in the `.githooks` directory.
To use them, run the following command after cloning this repository:

```sh
git config --local core.hooksPath .githooks
```

Guidelines
----------

- [Coding style](CODINGSTYLE.md) - Guidelines for writing APKBUILDs.
- [Commit style](COMMITSTYLE.md) - Guidelines for git commit messages.
