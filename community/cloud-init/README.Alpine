After the cloud-init package is installed you will need to run the
"setup-cloud-init" command to prepare the OS for cloud-init use.

This command will enable cloud-init's init.d services so that they are run
upon future boots/reboots.


** IMPORTANT **
---------------

To avoid the Alpine package having a large number of dependencies that in
many cases would be unnecessary the package only has a general set of
dependencies defined that are common for the majority of use cases.

When creating an Alpine OS image that makes use of cloud-init you should
read the rest of this document, set a DataSources list to only enable
whichever DataSource(s) you require, and install any additionally required
Alpine packages depending on the filesystem type, DataSource(s), etc in
use. Information related to this is contained in the rest of this document.


General configuration customisation
-----------------------------------

Whilst the /etc/cloud/cloud.cfg file can be modified it is better practice
to instead create a new file, or files, in the /etc/cloud/cloud.cfg.d/
directory to override settings from cloud.cfg.


Customising the enabled Data Sources list
-----------------------------------------

Whenever cloud-init runs on first boot it determines which DataSource is to
be used for performing machine configuration, it does so by reading the
"datasource_list" definition in either the /etc/cloud/cloud.cfg file or
other files in /etc/cloud/cloud.cfg.d/ directory.

By default this Alpine package sets "datasource_list" in the
/etc/cloud/cloud.cfg file with a list of all DataSources *except*
CloudSigma and SmartOS, this is because those 2 DataSources require the
py3-pyserial package to be installed, which is not defined as a dependency
of the Alpine cloud-init package.

If you are creating a cloud-init enabled Alpine image for a specific
environment, or a known set of environments, then it would make sense to
add a "datasource_list" value to only list whichever DataSource is
required/expected as this will potentially result in (slightly) faster
first boot times as each individually enabled DataSource would not have to
be checked in sequence until the required one is reached.

For example if creating an Alpine image only for use with AWS then creating
a file /etc/cloud/cloud.cfg.d/01-datasource.cfg with the following contents
would make sense:

  datasource_list: ['Ec2']

whereas if the image was intended to be used for either of AWS, Azure, or
Google Cloud then:

  datasource_list: ['Ec2', 'Azure', 'GCE', 'None']

and for an image to only be used with ISO or disk configuration files:

  datasource_list: ['NoCloud']


Additional Alpine packages required by specific DataSources
-----------------------------------------------------------

Cloud-init originally assumed the use of dhclient for DHCP purposes.
However as of cloud-init 23.3 there is some support for both dhcpcd and
udhcpc. Therefore no specific dhcp client software has been set as a hard
dependancy for the Alpine cloud-init package.

However if you are using a DataSource (i.e. AWS Ec2, Azure, GCE, Hetzner,
NWCS, OpenStack, Oracle, Scaleway, UpCloud, and Vultr) that needs to set up
an Ephemeral (temporary) DHCPv4 connection in order to talk to a Metadata
Server (to retrieve metadata, network configuration, and user-data
information from) then these DataSources may expect "dhclient" to be installed
and may try and execute it directly and therefore retrieving information
from the metadata server may fail if dhclient is not installed).

  Data Source                     Package(s) you should add
  ---------------------------------------------------------
  Azure                           dhclient, py3-passlib
  CloudSigma & SmartOS/Joyent     py3-pyserial
  Ec2                             dhclient, eudev
  GCE                             dhclient
  Hetzner                         dhclient
  MAAS                            py3-oauthlib
  NWCS                            dhclient
  OpenStack                       dhclient, eudev
  Oracle                          dhclient
  Scaleway                        dhclient
  UpCloud                         dhclient
  VMware                          py3-netifaces
  Vultr                           dhclient


Additional Alpine packages required by specific cloud-init modules
------------------------------------------------------------------

If you intend to use the following modules then you should install
additional Alpine packages:

  cc_ca_certs           ca-certificates

  cc_disk_setup         blockdev
                        lsblk
                        parted
                        sfdisk (for MBR partitioning)
                        sgdisk (for GPT partitioning)
                        wipefs (if you wish to delete existing partitions)

  cc_ssh_import_id	ssh-import-id

If you want to create/resize filesystems using cc_disk_setup and/or
cc_resizefs then you will need to install the relevant package(s)
containing the appropriate tools:

  BTRFS:        btrfs-progs
  EXT2/3/4:     e2fsprogs-extra
  F2FS:         f2fs-tools
  LUKS:         cryptsetup and device-mapper
  LVM:          lvm2 and device-mapper
  XFS:          xfsprogs and xfsprogs-extra
  ZFS:          zfs


Additional Alpine packages you may want or need to manually install
-------------------------------------------------------------------

  doas

    Alpine v3.15+, see the section "Sudo & Doas" below.

  mdevd or eudev

    see the section "mdev / mdevd / eudev" below.

  mount (Alpine Edge & v3.17+) or util-linux-misc (Alpine v3.15 & v3.16)
  or util-linux (Alpine <= v3.14)

    see the section "Using ISO images for cloud-init configuration (i.e.
    with NoCloud/ConfigDrive)" below.

  openssh-server or openssh-server-pam

    see the section "Unable to SSH in as user(s) as the account(s) is/are
    locked" below.

  sudo

    see the section "Sudo & Doas" below.


mdev / mdevd / eudev
--------------------

As of Alpine release v3.17 (and Edge from mid-Nov 2022 onwards) the Alpine
cloud-init package can be used together with either eudev (aka udev), mdev,
or mdevd.

Due to this change the cloud-init package from Alpine v3.17 onwards no
longer declares a dependency on the eudev package - you will need to
manually install either eudev or mdevd to your Alpine disk image if
required. You may also need to run "setup-devd mdevd" or "setup-devd udev"
accordingly to enable those instead of mdev.

Upstream cloud-init is only designed and tested to work with udev and
therefore some cloud-init functionality may not work when either mdev or
mdevd are used instead. This document will, over time, detail any known
mdev or mdevd incompatible cloud-init modules and/or Data Sources.


Sudo & Doas
-----------

Cloud-init has always had support for 'sudo' when adding users (via
user-data). As Alpine has now moved towards preferring the use of 'doas'
rather than 'sudo' support for 'doas' has been added to the cc_users_groups
module.

As a result, the Alpine cloud-init package from Alpine v3.15 onwards no
longer declares a dependency on sudo - you must ensure to install either
the 'doas' or 'sudo' package (or indeed both), depending on which you wish
to use, in order to be able to create users that can run commands as a
privileged user.


Doas
----

The cloud-init doas functionality performs some basic sanity checks of
per-user Doas rules - it double-checks that the user referred to in any
such rules corresponds to the user the rule(s) is/are defined for - if
not then an error appears during 1st boot and no Doas rules are added
for that user.

It is recommended that you set up a Doas rule: "permit persist :wheel" so
that all members of group 'wheel' can have root access. The default
/etc/doas.conf file has such a rule but it is commented out.

The cloud-init global configuration file /etc/cloud/cloud.cfg defines the
default 'alpine' user to be created upon 1st boot with a Doas rule giving
password-less root access - as the 'alpine' user is locked for password
access, both local and remote, and so only SSH key-based access is
available then obviously password-based Doas access would not work.

To setup Doas rules for additional users, both existing and new, in the
user-data add a 'doas' entry to define one or more rules to be setup for
each user, for example:

  users:
    - default
    - name: tester
      doas:
        - permit tester as root

When cloud-init runs on 1st boot it appends entries to the file
/etc/doas.conf containing all the per-user rules specified in
user-data, as well as the rule for the default 'alpine' user.

NOTE: if you specify a "users:" section in user-data you MUST specify a
"default" entry in addition to any other entries if you want the default
'alpine' user to be created.


NTP
---

It is recommended that you enable a NTP client on the machine. Cloud-init
supports both Chrony (fully featured) and Busybox's NTP client (a minimal
implementation).

Chrony is the default NTP client in cloud-init for Alpine Linux.


To use Chrony as the NTP client:

  Install the chrony package and enable the chrony init.d service

    # apk add chrony
    # rc-update add chronyd default

  Specify a ntp section in your cloud-init User Data like so:

    ntp:
      pools:
        - 0.uk.pool.ntp.org
        - 1.uk.pool.ntp.org

  If you do not specify any pools or servers then 0.pool.ntp.org ->
  3.pool.ntp.org will be used.

  The file /etc/cloud/templates/chrony.conf.alpine.tmpl is used by
  cloud-init as a template to create the configuration file
  /etc/chrony/chrony.conf.


To use Busybox as the NTP client:


  Edit the /etc/conf.d/ntpd file and change the line:

    NTPD_OPTS="-N -p pool.ntp.org"

  so that it is instead:

    NTPD_OPTS="-N"

  This changes the NTP client from using the hardcoded NTP server
  "pool.ntp.org" to instead use the /etc/ntp.conf file which will be
  generated by cloud-init upon first boot.

  Enable the ntp init.d service:

    # rc-update add ntpd default

  Specify a ntp section in your cloud-init User Data like so:

    ntp:
      ntp_client: ntp
      servers:
        - 192.168.0.1
        - 192.168.0.2

  If you do not specify any servers then 0.pool.ntp.org -> 3.pool.ntp.org
  will be used.

  The file /etc/cloud/templates/ntp.conf.alpine.tmpl is used by cloud-init
  as a template to create the configuration file /etc/ntp.conf.



Network interface hotplugging
-----------------------------

Version 21.3 of cloud-init has added some support for network interface
hotplugging, that is the addition or removal of additional network
interfaces on an already running machine.

A simple daemon, cloud-init-hotplugd, if enabled runs at boot time that
listens for udev hotplug events and triggers cloud-init to react to them.

This daemon, via its init.d script, is *not* at present enabled by the
setup-cloud-init script as hotplug is currently *only* supported by the
'ConfigDrive', 'Ec2', and 'OpenStack' DataSources.

In order to make use of network hotplug you will need to do the following
*three* things:

  - install and enable the Alpine "eudev" package for managing devices.

  - add the /etc/init.d/cloud-init-hotplugd script to the "default"
    run-level, i.e.

      rc-update add cloud-init-hotplugd default

  - enable hotplug for the relevant DataSource by adding the following to
    either the /etc/cloud.cfg file, or to a newly created file in the
    /etc/cloud.cfg.d directory, or else to the supplied user-data:

      updates:
        network:
          when: ['boot','hotplug']


Limitations
===========

Unable to resize LVM or LUKS rootfs partitions and filesystems on them
----------------------------------------------------------------------

cloud-init's cc_growpart module, if enabled, attempts to resize the root
partition. If however the rootfs is on a /dev/mapper/ devices (whether LVM,
or LUKS, or LVM-on-LUKS, or LUKS-on-LVM, cc_growpart will be unable to
resize the partition - this is due to a difference in behaviour between
mdev (always used by Alpine's initramfs **even if eudev is used by the rest
of the OS once the rootfs is mounted**) and eudev.

When the initramfs' init runs it will trigger mdev to create a /dev/mapper/
entry, if required, for the rootfs. With mdev this entry is an actual file,
whereas eudev would create it as a softlink to the underlying /dev/dm-*
device. Cloud-init's cc_growpart attempts to determine the device that
provides the rootfs and, due to this mdev limitation, believes this is
/dev/mapper/<whatever> rather that the actual /dev/dm-<whatever> device.
The growpart utility can resize /dev/dm-* devices.

This issues affects partition growing/fs resizing regardless of whether
udev, mdev, or mdevd is enabled in the Alpine disk image - Alpine's
initramfs' init *always* runs mdev regardless.

As a workaround, in user-data provided to the VM the runcmd module can be
used to perform any resizing as follows:

For a LUKS-encrypted rootfs (assuming it is on a LUKS partition labelled
"lukspart" and is device /dev/sda2):

  runcmd:
    - growpart /dev/sda2
    - cryptsetup resize lukspart
    - resize2fs /dev/mapper/lukspart

For a LVM-based rootfs (assuming it is on a LVM VG called "rootfs" in VG
"vg0" with /dev/sda2 being the partition LVM is using):

  runcmd:
    - growpart /dev/sda2
    - lvextend -l +100%FREE /dev/mapper/vg0-rootfs
    - resize2fs /dev/mapper/vg0-rootfs



Known Issues
============


Unable to SSH in as user(s) as the account(s) is/are locked
-----------------------------------------------------------

Issue: By default cloud-init will ensure that any user accounts have their
password locked. The OpenSSH sshd daemon has logic that, when PAM is not
enabled, for accounts with locked passwords it *also* decides to refuse
key-based SSH logins.

Solution: install the openssh-server-pam package (rather than
openssh-server) and edit /etc/ssh/sshd_config to ensure that it defines
"UsePAM yes".

In the future this package may add "openssh-server-pam" as a dependency but
as it is possible some individuals may wish to use cloud-init without any
SSH daemon installed that decision is unclear.


Using ISO images for cloud-init configuration (i.e. with NoCloud/ConfigDrive)
-----------------------------------------------------------------------------

The "mount" command in Alpine is provided by the "busybox" package by
default.

The "mount" package (Alpine v3.17+), "util-linux-misc" package (Alpine
v3.15 & v3.16), or "util-linux" package (Alpine <= v3.14) provides an
alternative, full featured, version of the "mount" command.

Cloud-init makes use of the mount command's "-t auto" option to mount a
filesystem containing cloud-init configuration data (detected by searching
for a filesystem with the label "cidata"). Busybox's mount command behaves
differently to that of util-linux's when the "-t auto" option is used,
specifically if the kernel module for the required filesystem is not
already loaded the util-linux mount command will trigger it to be loaded
and so the mount will succeed. However Busybox's mount will not normally
trigger a kernel module load and the mount will fail!

When this problem occurs a message such as the following will be displayed
on the console during boot:

  util.py[WARNING]: Failed to mount /dev/vdb when looking for data

If cloud-init debugging is enabled then the file /var/log/cloud-init.log
will also contain the following entries:

  subp.py[DEBUG]: Running command ['mount', '-o', 'ro', '-t', 'auto',
  '/dev/vdb', '/run/cloud-init/tmp/tmpAbCdEf'] with allowed return codes [0]
  (shell=False, capture=True)
  util.py[DEBUG]: Failed mount of '/dev/vdb' as 'auto': Unexpected error
  while running command.
  Command: ['mount', '-o', 'ro', '-t', 'auto', '/dev/vdb',
  '/run/cloud-init/tmp/tmpAbCdEf']
  Exit code: 255
  Reason: -
  Stdout:
  Stderr: mount: mounting /dev/vdb on /run/cloud-init/tmp/tmpAbCdEf failed:
  invalid argument

There are 2 possible solutions to this issue, either:

(1) Install the mount (Edge and v3.17+) / util-linux-misc (v3.15 & v3.16) /
    util-linux (<= v.3.14) package in the Alpine image used with
    cloud-init.

or:

(2) Create (or modify) the file /etc/filesystems and ensure it has a line
    present with the name of the required kernel module for the relevant
    filesystem i.e. "iso9660". This will ensure that Busybox's mount will
    trigger the loading of this kernel module.
