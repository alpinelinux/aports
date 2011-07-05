"""This provides Bcfg2 support for alpinelinux APK packages."""
__revision__ = '$Revision$'

import Bcfg2.Client.Tools

class APK(Bcfg2.Client.Tools.PkgTool):
    """apk package support."""
    name = 'APK'
    __execs__ = ["/sbin/apk"]
    __handles__ = [('Package', 'apk')]
    __req__ = {'Package': ['name', 'version']}
    pkgtype = 'apk'
    pkgtool = ("/sbin/apk add %s", ("%s", ["name"]))

    def __init__(self, logger, setup, config):
        Bcfg2.Client.Tools.PkgTool.__init__(self, logger, setup, config)
        self.installed = {}
        self.RefreshPackages()

    def RefreshPackages(self):
        """Refresh memory hashes of packages."""
        names = self.cmd.run("/sbin/apk info")[1]
        nameversions = self.cmd.run("/sbin/apk info -v")[1]
        for pkg in zip(names, nameversions):
                pkgname = pkg[0]
                version = pkg[1][len(pkgname)+1:]
                self.logger.debug(" pkgname: %s\n version: %s" % (pkgname, version))
                self.installed[pkgname] = version

    def VerifyPackage(self, entry, modlist):
        """Verify Package status for entry."""
        if not 'version' in entry.attrib:
            self.logger.info("Cannot verify unversioned package %s" %
               (entry.attrib['name']))
            return False

        if entry.attrib['name'] in self.installed:
            if entry.attrib['version'] == 'auto' or self.installed[entry.attrib['name']] == entry.attrib['version']:
                #if not self.setup['quick'] and \
                #                entry.get('verify', 'true') == 'true':
                #FIXME: We should be able to check this once
                #       http://trac.macports.org/ticket/15709 is implemented
                return True
            else:
		self.loggger.info( " pkg %s at version %s, not %s" % (entry.attrib['name'],self.installed[entry.attrib['name']],entry.attrib['version']) )
                entry.set('current_version', self.installed[entry.get('name')])
                return False
        entry.set('current_exists', 'false')
        return False

    def RemovePackages(self, packages):
        """Remove extra packages."""
        names = [pkg.get('name') for pkg in packages]
        self.logger.info("Removing packages: %s" % " ".join(names))
        self.cmd.run("/sbin/apk del %s" % \
                     " ".join(names))
        self.RefreshPackages()
        self.extra = self.FindExtraPackages()
