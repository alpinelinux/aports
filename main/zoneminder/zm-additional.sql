# Update settings to reflect alpine linux default install
update Config set Value = '/cgi-bin/zm/nph-zms' where Name = 'ZM_PATH_ZMS';
update Config set Value = '/var/log/zoneminder' where Name = 'ZM_PATH_LOGS';
update Config set Value = '/var/log/zoneminder/zm_debug_log+' where Name = 'ZM_EXTRA_DEBUG_LOG';
update Config set Value = '/var/log/zoneminder/zm_xml.log' where Name = 'ZM_EYEZM_LOG_FILE';
update Config set Value = '/var/run/zoneminder' where Name = 'ZM_PATH_SOCKS';
update Config set Value = '/var/lib/zoneminder/temp' where Name = 'ZM_PATH_SWAP';
update Config set Value = '/var/lib/zoneminder/temp' where Name = 'ZM_UPLOAD_FTP_LOC_DIR';
update Config set Value = '/var/lib/zoneminder/temp' where Name = 'ZM_UPLOAD_LOC_DIR';
update Config set Value = '/var/lib/zoneminder/events' where Name = 'ZM_DIR_EVENTS';
update Config set Value = '/var/lib/zoneminder/images' where Name = 'ZM_DIR_IMAGES';
update Config set Value = '/var/lib/zoneminder/sounds' where Name = 'ZM_DIR_SOUNDS';
update Config set Value = '-5' where Name = 'ZM_LOG_LEVEL_SYSLOG';
update Config set Value = '0' where Name = 'ZM_LOG_LEVEL_FILE';
