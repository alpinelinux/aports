# Update settings to reflect alpine linux default install
update Config set Value = '/zm/cgi-bin/nph-zms' where Name = 'ZM_PATH_ZMS';
update Config set Value = '/var/log/zoneminder' where Name = 'ZM_PATH_LOGS';
update Config set Value = '/var/log/zoneminder/zm_debug_log+' where Name = 'ZM_EXTRA_DEBUG_LOG';
update Config set Value = '/var/log/zoneminder/zm_xml.log' where Name = 'ZM_EYEZM_LOG_FILE';
update Config set Value = '/var/lib/zoneminder/sock' where Name = 'ZM_PATH_SOCKS';
update Config set Value = '/var/lib/zoneminder/swap' where Name = 'ZM_PATH_SWAP';
update Config set Value = '/var/spool/zoneminder-upload' where Name = 'ZM_UPLOAD_FTP_LOC_DIR';
