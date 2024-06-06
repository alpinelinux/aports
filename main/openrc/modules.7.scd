modules(7)

# NAME

modules - Loads explicitly defined kernel modules

# DESCRIPTION

The *modules* service loads explicitly defined kernel modules during early
startup. This service is enabled by default.

The modules service will read configuration files matching the following
patterns:

- _/lib/modules-load.d/\*.conf_
- _/usr/lib/modules-load\.d/\*.conf_
- _/etc/modules_
- _/etc/modules-load.d/\*.conf_
- _/run/modules-load.d/\*.conf_

If multiple files with the same name are found in different locations, only the
one in the latter location is processed.

# FILE FORMAT

Files must contain exactly one module name per line. Empty lines and lines
starting with # are ignored.

# SEE ALSO

modprobe(8)
