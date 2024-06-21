<!--
This is the issue template for reporting an issue with a specific package. You
can select a different issue template from the dropdown above. Also, feel free
to use the "No template" option in case no template applies to your issue.

Also note that this repository is intended for reporting issues with packages.
For other components, separate issue trackers exist:

    * Installer issues: https://gitlab.alpinelinux.org/alpine/alpine-conf/-/issues
    * Infrastructure issues: https://gitlab.alpinelinux.org/alpine/infra/infra/-/issues
    * Initramfs issues: https://gitlab.alpinelinux.org/alpine/mkinitfs/-/issues
-->

## Package Information

* Package name: *Enter the apk package name*
* Package version: *Version as obtained by `apk info`*
* Alpine version: *Alpine version from `cat /etc/alpine-release`*
* Alpine architecture: *Architecture obtained via `apk --print-arch`*

## Summary

*Write a brief description of your issue with the selected package.*

## Steps to reproduce

*If applicable, please provide instructions to reproduce the issue.*

/label ~type:bug
/label ~triage:pending
