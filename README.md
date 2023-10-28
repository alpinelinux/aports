# Alpine Linux aports repository

This repository contains the APKBUILD files for each and every
Alpine Linux package, along with the required patches and scripts,
if any.

It also contains some extra files and directories related to testing
(and therefore, building) those packages on GitLab (via GitLab CI).

If you want to contribute, please read the
[contributor guide](https://wiki.alpinelinux.org/wiki/Alpine_Linux:Contribute)
and feel free to either submit a merge request on
[GitLab](https://gitlab.alpinelinux.org/alpine/aports),
or to submit a git patch on the Alpine aports mailing list
([~alpine/aports@lists.alpinelinux.org](mailto:~alpine/aports@lists.alpinelinux.org)). (note: this mailing list is presently not functional)

## Repositories

The Alpine Linux aports tree consists of 3 repositories (directories). Each of
these 3 repositories have its own set of policies, use cases and workflows.
Below is a definition of the basic policies your package should apply to.
Additional policies could apply, please refer to our developer guidelines.

### main

Packages in the main repository should be supported following our official
release cycle documentation as defined on our
[website](https://alpinelinux.org/releases/). In case of doubt a package should
be moved to our community repository instead. The policy for a package in the
main repository is if this package is reasonable to be expected in a basic
system and has a developer assigned to it who can maintain it as documented on
our release page. A package in main is also expected to include proper
documentation if shipped with the source code and have test suites enabled if
provided. New packages are rarely introduced directly into the main repository
and should follow the workflow: `testing => main`.

### community

Packages in the community repository should be supported following our official
release cycle documentation as defined on our
[website](https://alpinelinux.org/releases/). Packages in community are those
that do not belong in our main repository and have finished testing in our
testing repository. A package should have a maintainer and have test suites
enabled if provided and is preferred to ship documentation if the source code
provides it. New packages are rarely introduced directly into the community
repository and should follow the workflow: `testing => community`

### testing

Packages in the testing repositories do **not** follow our official release
cycle documentation as defined on our
[website](https://alpinelinux.org/releases/) and are **not** included in our
official releases and are only shipped in our edge branch. This repository is
specifically designed to introduce and test packages and as a staging area for
our other repositories. The packages do not follow any of the previously
mentioned policies and only need to be able to be build correctly. After the
package is verified to be working it should be moved to one of the other
repositories as soon as possible following the policies set for that repository.
If the package is not moved within a 6 month period we will notify the
maintainer and remove it after 9 months.

## Git Hooks

You can find some useful git hooks in the `.githooks` directory.
To use them, run the following command after cloning this repository:

```sh
git config --local core.hooksPath .githooks
```

## Guidelines

- [Coding style](CODINGSTYLE.md) - Guidelines for writing APKBUILDs.
- [Commit style](COMMITSTYLE.md) - Guidelines for git commit messages.
