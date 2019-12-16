<?php

/*
 +-----------------------------------------------------------------------+
 | Local configuration for the Roundcube Webmail installation.           |
 |                                                                       |
 | This is a samle configuration file only containing the most common    |
 | configuration options. You may copy more options from                 |
 | defaults.inc.php to this file to override the defaults.               |
 |                                                                       |
 | This file is provided by Alpine Linux.                                |
 +-----------------------------------------------------------------------+
*/

$config = array();

// ----------------------------------
// SQL DATABASE
// ----------------------------------

// Database connection string (DSN) for read+write operations.
// Format (compatible with PEAR MDB2): db_provider://user:password@host/database
// Currently supported db_providers: mysql, pgsql, sqlite, mssql, sqlsrv.
// IMPORTANT: Install package for the DB of your choice, e.g. `apk add roundcubemail-pgsql`.
$config['db_dsnw'] = 'pgsql://roundcube:top-secret@localhost/roundcube';


// ----------------------------------
// IMAP
// ----------------------------------

// The IMAP host chosen to perform the log-in.
// Leave blank to show a textbox at login, give a list of hosts
// to display a pulldown menu or set one host as string.
// To use SSL/TLS connection, enter hostname with prefix ssl:// or tls://
//$config['default_host'] = 'localhost';

// TCP port used for IMAP connections
//$config['default_port'] = 143;

// IMAP connection timeout, in seconds. Default: 0 (use default_socket_timeout)
//$config['imap_timeout'] = 0;


// ----------------------------------
// SMTP
// ----------------------------------

// SMTP server host (for sending mails).
// Enter hostname with prefix tls:// to use STARTTLS, or use
// prefix ssl:// to use the deprecated SSL over SMTP (aka SMTPS).
//$config['smtp_server'] = 'localhost';

// SMTP port (default is 25; use 587 for STARTTLS or 465 for the
// deprecated SSL over SMTP (aka SMTPS)).
//$config['smtp_port'] = 25;

// SMTP username (if required) if you use %u as the username Roundcube
// will use the current username for login.
//$config['smtp_user'] = '%u';

// SMTP password (if required) if you use %p as the password Roundcube
// will use the current user's password for login.
//$config['smtp_pass'] = '%p';


// ----------------------------------
// SYSTEM
// ----------------------------------

// Provide an URL where a user can get support for this Roundcube installation.
// PLEASE DO NOT LINK TO THE ROUNDCUBE.NET WEBSITE HERE!
//$config['support_url'] = '';

// Allow browser-autocompletion on login form.
// 0 - disabled, 1 - username and host only, 2 - username, host, password
//$config['login_autocomplete'] = 0;

// Session lifetime in minutes.
//$config['session_lifetime'] = 10;

// Name your service. This is displayed on the login screen and in the window title.
//$config['product_name'] = 'Roundcube Webmail';

// Load the key from /etc/roundcube/session_key (random key generated on install).
// This key is used to encrypt the users imap password which is stored
// in the session record (and the client cookie if remember password is enabled).
// It must be exactly 24 chars long.
$config['des_key'] = trim(file(RCMAIL_CONFIG_DIR . '/session_key')[0]);

// Password charset.
// Defaults to ISO-8859-1 for backward compatibility
$config['password_charset'] = 'UTF-8';

// Mimetypes supported by the browser.
// Attachments of these types will open in a preview window.
// Either a comma-separated list or an array: 'text/plain,text/html,text/xml,image/jpeg,image/gif,image/png,application/pdf'.
$config['client_mimetypes'] = 'text/plain,text/html,image/jpeg,image/gif,image/png,application/pdf';

// List of active plugins (in plugins/ directory).
//$config['plugins'] = array();


// ----------------------------------
// USER INTERFACE
// ----------------------------------

// Make use of the built-in spell checker.
//$config['enable_spellcheck'] = false;

// Set the spell checking engine. Possible values:
// - 'googie'  - don't use this, it sends everything you type to a remote service!
// - 'pspell'  - requires the PHP Pspell module and aspell installed (apk add php7-pspell)
// - 'enchant' - requires the PHP Enchant module (apk add php7-enchant)
// - 'atd'     - install your own After the Deadline server or check with the people at http://www.afterthedeadline.com before using their API
//$config['spellcheck_engine'] = 'pspell';

// Enables files upload indicator. Requires APC installed (apk add php7-apcu)
// and enabled apc.rfc1867 option.
// By default refresh time is set to 1 second. You can set this value to true
// or any integer value indicating number of seconds.
//$config['upload_progress'] = false;


// ----------------------------------
// USER PREFERENCES
// ----------------------------------

// Use this charset as fallback for message decoding
$config['default_charset'] = 'UTF-8';

// display remote inline images
// 0 - Never, always ask
// 1 - Ask if sender is not in address book
// 2 - Always show inline images
//$config['show_images'] = 0;

// save compose message every 300 seconds (5min)
//$config['draft_autosave'] = 300;

// When replying:
// -1 - don't cite the original message
// 0  - place cursor below the original message
// 1  - place cursor above original message (top posting)
//$config['reply_mode'] = 0;
