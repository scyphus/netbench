/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include <netbench.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, const char *const argv[])
{
    int ret;
    nb_ping_t *ping;
    double t0;
    double t1;

    t0 = nb_microtime();

    /* Prepare the ping measurement */
    ping = nb_ping_open(AF_INET);
    if ( NULL == ping ) {
        /* Cannot open ping socket */
        fprintf(stderr, "Cannot prepare ping measurement.\n");
        return EXIT_FAILURE;
    }

    ret = nb_ping_exec(ping, "157.82.3.4", 12, 10, 1.0, 3.0);
    if ( 0 != ret ) {
        /* Cannot execute the ping measurement */
        fprintf(stderr, "Cannot execute ping measurement.\n");
        return EXIT_FAILURE;
    }

    nb_ping_close(ping);

    t1 = nb_microtime();


    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
