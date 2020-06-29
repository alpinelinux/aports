#    Copyright (c) 2020 Dermot Bradley
#
#    Author: Dermot Bradley <dermot_bradley@yahoo.com>
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
#
#    Alternatively, this program may be used under the terms of the Apache
#    License, Version 2.0, in which case the provisions of that license are
#    applicable instead of those above.  If you wish to allow use of your
#    version of this program under the terms of the Apache License, Version 2.0
#    only, indicate your decision by deleting the provisions above and replace
#    them with the notice and other provisions required by the Apache License,
#    Version 2.0. If you do not delete the provisions above, a recipient may
#    use your version of this file under the terms of either the GPLv3 or the
#    Apache License, Version 2.0.

"""
Apk Configure
-------------
**Summary:** configure apk

This module handles configuration of the /etc/apk/repositories file.

.. note::
    To ensure that apk configuration is valid yaml, any strings containing
    special characters, especially ``:`` should be quoted.

**Preserve repositories:**

By default, cloud-init will generate a new repositories file based on any
configuration specified in cloud config. To disable this behavior and preserve
the existing repositories file set ``preserve_repositories`` to ``true``.

.. note::
    The ``preserve_repositories`` option overrides all other config keys that
    would alter the repositories file.

**Specify repositories entries:**

The alpine_repo entry under ``apk_repos`` is a dictionary which contains the
following keys:

    - ``version``: the version of Alpine (either "vX.Y" or "edge")
    - ``base_url``: the base URL (up to, but not including, the version) of an
                    Alpine mirror to use (optional)
    - ``community_enabled``: whether to enable the Community repository
                             (optional, defaults to false)
    - ``testing_enabled``: whether to enable the Testing repository (optional,
                           defaults to false)

The local_repo entry under ``apk_repos`` is an optional dictionary which
contains the following key:

    - ``base_url``: the base URL (up to, but not including, the architecture
                    name) of a repository of unofficial Alpine packages

**Internal name:** ``cc_apk_configure``

**Module frequency:** per instance

**Supported distros:** alpine

**Config keys**::

    apk_repos:
        preserve_repositories: <true/false>
        alpine_repo:
            - version: "<alpine version>"
              base_url: "<repo base url>"
              community_enabled: <true/false>
              testing_enabled: <true/false>
        local_repo:
            - base_url: "<repo base url>"
"""

import os

from cloudinit import log as logging
from cloudinit import util

distros = ['alpine']


def _write_repositories_file(alpine_repo, local_baseurl, log):
    repo_file = '/etc/apk/repositories'

    alpine_baseurl = alpine_repo.get('base_url')
    if not alpine_baseurl:
        # Default Alpine mirror to use
        alpine_baseurl = 'https://alpine.global.ssl.fastly.net/alpine'

    alpine_version = alpine_repo.get('version')

    repo_config = '#\n# Created by cloud-init\n#\n'
    repo_config += '# This file is written on first boot of an instance\n#\n'

    repo_config += "\n%s/%s/main" % (alpine_baseurl, alpine_version)
    if alpine_repo.get('community_enabled'):
        repo_config += "\n%s/%s/community" % (alpine_baseurl, alpine_version)
    if alpine_repo.get('testing_enabled'):
        repo_config += "\n\n@testing %s/edge/testing" % alpine_baseurl

    if local_baseurl != '':
        repo_config += "\n\n@local %s\n" % local_baseurl
    util.write_file(repo_file, repo_config)


def handle(name, cfg, cloud, log, _args):

    apk_section = cfg.get('apk_repos')
    if not apk_section:
        log.debug(("Skipping module named %s,"
                   " no 'apk_repos' section found"), name)
        return

    if util.is_true(apk_section.get('preserve_repositories'), False):
        log.debug(("Skipping module named %s,"
                   " 'preserve_repositories' is set"), name)
        return

    alpine_repo = apk_section.get('alpine_repo')
    if not alpine_repo:
        log.debug(("Skipping module named %s,"
                   " no 'alpine_repo' configuration found"), name)
        return

    alpine_version = alpine_repo.get('version')
    if not alpine_version:
        log.debug(("Skipping module named %s,"
                   " 'version' not specified in alpine_repo"), name)
        return

    local_repo = apk_section.get('local_repo', {})
    local_baseurl = local_repo.get('base_url')
    
    _write_repositories_file(alpine_repo, local_baseurl, log)

# vi: ts=4 expandtab
