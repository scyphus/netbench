/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include "netbench_private.h"
#include <netbench.h>
#include <sys/time.h>

/*
 * Get current time in microtime
 */
double
nb_microtime(void)
{
    struct timeval tv;
    double microsec;

    if ( 0 != gettimeofday(&tv, NULL) ) {
        return 0.0;
    }

    microsec = (double)tv.tv_sec + (1.0 * tv.tv_usec / 1000000);

    return microsec;
}

/*
 * Calculate checksum
 */
uint16_t
nb_checksum(const uint8_t *buf, size_t len) {
    size_t nleft;
    int32_t sum;
    const uint16_t *cur;
    union {
        uint16_t us;
        uint8_t uc[2];
    } last;
    uint16_t ret;

    nleft = len;
    sum = 0;
    cur = (const uint16_t *)buf;

    while ( nleft > 1 ) {
        sum += *cur;
        cur += 1;
        nleft -= 2;
    }

    if ( 1 == nleft ) {
        last.uc[0] = *(const uint8_t *)cur;
        last.uc[1] = 0;
        sum += last.us;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    ret = ~sum;

    return ret;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
