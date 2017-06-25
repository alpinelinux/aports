<?php
$CONFIG = array (
  'datadirectory' => '/var/lib/nextcloud/data',
  'logfile' => '/var/log/nextcloud/nextcloud.log',
  'apps_paths' => array (
    // Read-only location for apps shipped with Nextcloud and installed by apk.
    0 => array (
      'path' => '/usr/share/webapps/nextcloud/apps',
      'url' => '/apps',
      'writable' => false,
    ),
    // Writable location for apps installed from AppStore.
    1 => array (
      'path' => '/var/lib/nextcloud/apps',
      'url' => '/apps-appstore',
      'writable' => true,
    ),
  ),
  'updatechecker' => false,
  'check_for_working_htaccess' => false,

  // Uncomment to enable Zend OPcache.
  //'memcache.local' => '\OC\Memcache\APCu',

  'installed' => false,
);
