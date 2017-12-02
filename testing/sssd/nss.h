#ifndef NSS__H
#define NSS__H

#include <nss/nss.h>

enum nss_status
{
    NSS_STATUS_TRYAGAIN = -2,
    NSS_STATUS_UNAVAIL = -1,
    NSS_STATUS_NOTFOUND = 0,
    NSS_STATUS_SUCCESS = 1,
    NSS_STATUS_RETURN = 2
};

#endif
