dnl enable ipv6
APPENDDEF(`conf_libmilter_ENVDEF',`-DNETINET6=1')
dnl getipnodebyname/getipnodebyaddr is deprecated and not part of musl libc
APPENDDEF(`conf_libmilter_ENVDEF',`-DNEEDSGETIPNODE=1')
