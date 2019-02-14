#    Copyright (C) 2016 Matt Dainty
#
#    Author: Matt Dainty <matt@bodgit-n-scarper.com>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License version 3, as
#    published by the Free Software Foundation.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

from cloudinit import distros
from cloudinit import helpers
from cloudinit import log as logging
from cloudinit import util

from cloudinit.distros.parsers.hostname import HostnameConf

from cloudinit.settings import PER_INSTANCE

LOG = logging.getLogger(__name__)


class Distro(distros.Distro):
    network_conf_fn = "/etc/network/interfaces"
    init_cmd = ['rc-service']  # init scripts

    def __init__(self, name, cfg, paths):
        distros.Distro.__init__(self, name, cfg, paths)
        # This will be used to restrict certain
        # calls from repeatly happening (when they
        # should only happen say once per instance...)
        self._runner = helpers.Runners(paths)
        self.osfamily = 'alpine'
        cfg['ssh_svcname'] = 'sshd'

    def apply_locale(self, locale, out_fn=None):
        # No locale support yet
        pass

    def install_packages(self, pkglist):
        self.update_package_sources()
        self.package_command('add', pkgs=pkglist)

    def _write_network(self, settings):
        util.write_file(self.network_conf_fn, settings)
        return ['all']

    def _bring_up_interfaces(self, device_names):
        use_all = False
        for d in device_names:
            if d == 'all':
                use_all = True
        if use_all:
            return distros.Distro._bring_up_interface(self, '--all')
        else:
            return distros.Distro._bring_up_interfaces(self, device_names)

    def _select_hostname(self, hostname, fqdn):
        # Prefer the short hostname over the long
        # fully qualified domain name
        if not hostname:
            return fqdn
        return hostname

    def _write_hostname(self, your_hostname, out_fn):
        conf = None
        try:
            # Try to update the previous one
            # so lets see if we can read it first.
            conf = self._read_hostname_conf(out_fn)
        except IOError:
            pass
        if not conf:
            conf = HostnameConf('')
        conf.set_hostname(your_hostname)
        util.write_file(out_fn, str(conf), 0o644)

    def _read_system_hostname(self):
        sys_hostname = self._read_hostname(self.hostname_conf_fn)
        return (self.hostname_conf_fn, sys_hostname)

    def _read_hostname_conf(self, filename):
        conf = HostnameConf(util.load_file(filename))
        conf.parse()
        return conf

    def _read_hostname(self, filename, default=None):
        hostname = None
        try:
            conf = self._read_hostname_conf(filename)
            hostname = conf.hostname
        except IOError:
            pass
        if not hostname:
            return default
        return hostname

    def set_timezone(self, tz):
        distros.set_etc_timezone(tz=tz, tz_file=self._find_tz_file(tz))

    def package_command(self, command, args=None, pkgs=None):
        if pkgs is None:
            pkgs = []

        cmd = ['apk']
        # Redirect output
        cmd.append("--quiet")

        if args and isinstance(args, str):
            cmd.append(args)
        elif args and isinstance(args, list):
            cmd.extend(args)

        if command:
            cmd.append(command)

        pkglist = util.expand_package_list('%s-%s', pkgs)
        cmd.extend(pkglist)

        # Allow the output of this to flow outwards (ie not be captured)
        util.subp(cmd, capture=False)

    def update_package_sources(self):
        self._runner.run("update-sources", self.package_command,
                         ["update"], freq=PER_INSTANCE)

    def add_user(self, name, **kwargs):
        if util.is_user(name):
            LOG.info("User %s already exists, skipping." % name)
            return

        adduser_cmd = ['adduser', name, '-D']
        log_adduser_cmd = ['adduser', name, '-D']

        # Since we are creating users, we want to carefully validate the
        # inputs. If something goes wrong, we can end up with a system
        # that nobody can login to.
        adduser_opts = {
            "gecos": '-g',
            "homedir": '-h',
            "uid": '-u',
            "shell": '-s',
        }

        adduser_flags = {
            "system": '-S',
        }

        redact_opts = ['passwd']

        # Check the values and create the command
        for key, val in kwargs.items():

            if key in adduser_opts and val and isinstance(val, str):
                adduser_cmd.extend([adduser_opts[key], val])

                # Redact certain fields from the logs
                if key in redact_opts:
                    log_adduser_cmd.extend([adduser_opts[key], 'REDACTED'])
                else:
                    log_adduser_cmd.extend([adduser_opts[key], val])

            elif key in adduser_flags and val:
                adduser_cmd.append(adduser_flags[key])
                log_adduser_cmd.append(adduser_flags[key])

        # Don't create the home directory if directed so or if the user is a
        # system user
        if 'no_create_home' in kwargs or 'system' in kwargs:
            adduser_cmd.append('-H')
            log_adduser_cmd.append('-H')

        # Run the command
        LOG.debug("Adding user %s", name)
        try:
            util.subp(adduser_cmd, logstring=log_adduser_cmd)
        except Exception as e:
            util.logexc(LOG, "Failed to create user %s", name)
            raise e

        # Unlock the user
        LOG.debug("Unlocking user %s", name)
        try:
            util.subp(['passwd', '-u', name], logstring=['passwd', '-u', name])
        except Exception as e:
            util.logexc(LOG, "Failed to unlock user %s", name)
            raise e

        if 'groups' in kwargs:
            groups = kwargs['groups']
            if groups and isinstance(groups, str):
                # Why are these even a single string in the first place?
                groups = groups.split(',')
            for group in groups:
                try:
                    util.subp(['adduser', name, group], logstring=['adduser', name, group])
                except Exception as e:
                    util.logexc(LOG, "Failed to add user %s to group %s", name, group)
                    raise e
