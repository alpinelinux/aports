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

"""Apk Configure: Configures apk repositories file."""

from textwrap import dedent

from cloudinit import log as logging
from cloudinit import util
from cloudinit.config.schema import (
    get_schema_doc, validate_cloudconfig_schema)
from cloudinit.settings import PER_INSTANCE

LOG = logging.getLogger(__name__)


# If no mirror is specified then use this one
DEFAULT_MIRROR = "https://alpine.global.ssl.fastly.net/alpine"


frequency = PER_INSTANCE
distros = ['alpine']
schema = {
    'id': 'cc_apk_configure',
    'name': 'APK Configure',
    'title': 'Configure apk repositories file',
    'description': dedent("""\
        This module handles configuration of the /etc/apk/repositories file.

        .. note::
          To ensure that apk configuration is valid yaml, any strings
          containing special characters, especially ``:`` should be quoted.
    """),
    'distros': distros,
    'examples': [
        dedent("""\
        # Keep the existing /etc/apk/repositories file unaltered.
        apk_repos:
            preserve_repositories: true
        """),
        dedent("""\
        # Create repositories file for Alpine v3.12 main and community
        # using default mirror site.
        apk_repos:
            preserve_repositories: false
            alpine_repo:
                community_enabled: true
                version: 'v3.12'
        """),
        dedent("""\
        # Create repositories file for Alpine Edge main, community, and
        # testing using a specified mirror site and also a local repo.
        apk_repos:
            preserve_repositories: false
            alpine_repo:
                base_url: 'https://some-alpine-mirror/alpine'
                community_enabled: true
                testing_enabled: true
                version: 'edge'
            local_repo_base_url: 'https://my-local-server/local-alpine'
        """),
    ],
    'frequency': frequency,
    'type': 'object',
    'properties': {
        'apk_repos': {
            'type': 'object',
            'properties': {
                'preserve_repositories': {
                    'type': 'boolean',
                    'default': False,
                    'description': dedent("""\
                        By default, cloud-init will generate a new repositories
                        file ``/etc/apk/repositories`` based on any
                        settings specified in cloud config. To disable this
                        behavior and preserve the repositories file from the
                        pristine image, set ``preserve_repositories``
                        to ``true``.

                        The ``preserve_repositories`` option overrides
                        all other config keys that would alter
                        ``/etc/apk/repositories``.
                    """)
                },
                'alpine_repo': {
                    'type': ['object', 'null'],
                    'properties': {
                        'base_url': {
                            'type': 'string',
                            'default': DEFAULT_MIRROR,
                            'description': dedent("""\
                                The base URL of an Alpine repository, or
                                mirror, to download official packages from.
                                If not specified then it defaults to ``{}``
                            """.format(DEFAULT_MIRROR))
                        },
                        'community_enabled': {
                            'type': 'boolean',
                            'default': False,
                            'description': dedent("""\
                                Whether to add the Community repo to the
                                repositories file. By default the Community
                                repo is not included.
                            """)
                        },
                        'testing_enabled': {
                            'type': 'boolean',
                            'default': False,
                            'description': dedent("""\
                                Whether to add the Testing repo to the
                                repositories file. By default the Testing
                                repo is not included. It is only recommended
                                to use the Testing repo on a machine running
                                the ``Edge`` version of Alpine as packages
                                installed from Testing may have dependancies
                                that conflict with those in non-Edge Main or
                                Community repos."
                            """)
                        },
                        'version': {
                            'type': 'string',
                            'description': dedent("""\
                                The Alpine version to use (e.g. ``v3.12`` or
                                ``edge``)
                            """)
                        },
                    },
                    'required': ['version'],
                    'minProperties': 1,
                    'additionalProperties': False,
                },
                'local_repo_base_url': {
                    'type': 'string',
                    'description': dedent("""\
                        The base URL of an Alpine repository containing
                        unofficial packages
                    """)
                }
            },
            'required': [],
            'minProperties': 1, # Either preserve_repositories or alpine_repo
            'additionalProperties': False,
        }
    }
}

__doc__ = get_schema_doc(schema)


def handle(name, cfg, cloud, log, _args):
    """
    Call to handle apk_repos sections in cloud-config file.

    @param name: The module name "apk-configure" from cloud.cfg
    @param cfg: A nested dict containing the entire cloud config contents.
    @param cloud: The L{CloudInit} object in use.
    @param log: Pre-initialized Python logger object to use for logging.
    @param _args: Any module arguments from cloud.cfg
    """

    # If there is no "apk_repos" section in the configuration
    # then do nothing.
    apk_section = cfg.get('apk_repos')
    if not apk_section:
        LOG.debug(("Skipping module named %s,"
                   " no 'apk_repos' section found"), name)
        return

    validate_cloudconfig_schema(cfg, schema)

    # If "preserve_repositories" is explicitly set to True in
    # the configuration do nothing.
    if util.is_true(apk_section.get('preserve_repositories'), False):
        LOG.debug(("Skipping module named %s,"
                   " 'preserve_repositories' is set"), name)
        return

    # If there is no "alpine_repo" subsection of "apk_repos" present in the
    # configuration then do nothing, as at least "version" is required to
    # create valid repositories entries.
    alpine_repo = apk_section.get('alpine_repo')
    if not alpine_repo:
        LOG.debug(("Skipping module named %s,"
                   " no 'alpine_repo' configuration found"), name)
        return

    # If there is no "version" value present in configuration then do nothing.
    alpine_version = alpine_repo.get('version')
    if not alpine_version:
        LOG.debug(("Skipping module named %s,"
                   " 'version' not specified in alpine_repo"), name)
        return

    local_repo = apk_section.get('local_repo_base_url', '')

    _write_repositories_file(alpine_repo, alpine_version, local_repo)


def _write_repositories_file(alpine_repo, alpine_version, local_repo):
    """
    Write the /etc/apk/repositories file with the specified entries.

    @param alpine_repo: A nested dict of the alpine_repo configuration.
    @param alpine_version: A string of the Alpine version to use.
    @param local_repo: A string containing the base URL of a local repo.
    """

    repo_file = '/etc/apk/repositories'

    alpine_baseurl = alpine_repo.get('base_url', DEFAULT_MIRROR)

    repo_config = '#\n# Created by cloud-init\n#\n'
    repo_config += '# This file is written on first boot of an instance\n#\n'

    repo_config += "\n%s/%s/main\n" % (alpine_baseurl, alpine_version)
    LOG.debug('Placing main section in the generated configuration')
    if alpine_repo.get('community_enabled'):
        LOG.debug('Adding community section to the generated configuration')
        repo_config += "%s/%s/community\n" % (alpine_baseurl, alpine_version)
    if alpine_repo.get('testing_enabled'):
        if alpine_version != 'edge':
            repo_config += "\n#\n# Testing - using this with a non-Edge "
            repo_config += "installation will likely cause problems!\n#\n"
        LOG.debug('Adding testing section to the generated configuration')
        repo_config += "%s/edge/testing\n" % alpine_baseurl

    if local_repo != '':
        LOG.debug('Adding local repo section to the generated configuration')
        repo_config += "\n#\n# Local repo\n#"
        repo_config += "\n%s/%s\n" % (local_repo, alpine_version)

    repo_config += '\n'
    util.write_file(repo_file, repo_config)

# vi: ts=4 expandtab
