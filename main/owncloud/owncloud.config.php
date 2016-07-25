<?php
$CONFIG = array(
  'datadirectory' => '/var/lib/owncloud/data',
  'apps_path' => array (
    0 => array (
      "path" => OC::$SERVERROOT."/apps",
      "url" => "/apps",
      "writable" => false,
    ),
    /* Uncomment this and install your apps here
    1 => array (
      "path" => "/var/www/localhost/htdocs/myoc_apps",
      "url" => "/myoc_apps",
      "writable" = true,
    ),
    */
  ),
  'version' => '9.1.0',
  'dbname' => 'owncloud',
  'dbhost' => 'localhost',
  'dbuser' => 'owncloud',
  'installed' => false,
  'loglevel' => '0',
);
?>
