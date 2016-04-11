# Update settings to reflect alpine linux default install
update Config set Value = '/cgi-bin/zm/nph-zms' where Name = 'ZM_PATH_ZMS';
update Config set Value = '-5' where Name = 'ZM_LOG_LEVEL_SYSLOG';
update Config set Value = '0' where Name = 'ZM_LOG_LEVEL_FILE';
