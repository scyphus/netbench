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

void
cb_ping(nb_ping_t *ping, int seq, double rtt)
{
    printf("RTT = %lf ms (Seq = #%d)\n", rtt * 1000, seq);
}
void
cb_traceroute(nb_traceroute_t *tr, int ttl,
              const struct sockaddr_storage *saddr, socklen_t saddrlen,
              double rtt)
{
    char tmphost[NI_MAXHOST];
    char tmpservice[NI_MAXSERV];

    if ( 0 != getnameinfo((struct sockaddr *)saddr, saddrlen, tmphost,
                          sizeof(tmphost), tmpservice, sizeof(tmpservice),
                          NI_NUMERICHOST | NI_NUMERICSERV) ) {
        tmphost[0] = '\0';
    }
    printf("%d: %s %lf ms\n", ttl, tmphost, rtt * 1000);
}

int
main(int argc, const char *const argv[])
{
    int ret;
    nb_ping_t *ping;
    nb_traceroute_t *tr;

    /* Prepare the ping measurement (IPv40 */
    printf("Starting ping (IPv4)...\n");
    ping = nb_ping_open(AF_INET);
    if ( NULL == ping ) {
        /* Cannot open ping socket */
        fprintf(stderr, "Cannot prepare ping measurement.\n");
        return EXIT_FAILURE;
    }

    /* Set a callback function */
    (void)nb_ping_set_callback(ping, cb_ping, NULL);

    /* Execute ping */
    ret = nb_ping_exec(ping, "netsurvey.jar.jp", 12, 10, 1.0, 3.0);
    if ( 0 != ret ) {
        /* Cannot execute the ping measurement */
        fprintf(stderr, "Cannot execute ping measurement.\n");
        return EXIT_FAILURE;
    }

    nb_ping_close(ping);
    printf("Finishied ping (IPv4).\n");


    /* Prepare the ping measurement (IPv6) */
    printf("Starting ping (IPv6)...\n");
    ping = nb_ping_open(AF_INET6);
    if ( NULL == ping ) {
        /* Cannot open ping socket */
        fprintf(stderr, "Cannot prepare ping measurement.\n");
        return EXIT_FAILURE;
    }

    /* Set a callback function */
    (void)nb_ping_set_callback(ping, cb_ping, NULL);

    /* Execute ping */
    ret = nb_ping_exec(ping, "netsurvey.jar.jp", 2, 10, 1.0, 3.0);
    if ( 0 != ret ) {
        /* Cannot execute the ping measurement */
        fprintf(stderr, "Cannot execute ping measurement.\n");
        return EXIT_FAILURE;
    }

    nb_ping_close(ping);
    printf("Finishied ping (IPv6).\n");


    /* Prepare the traceroute measurement (IPv6) */
    printf("Starting traceroute...\n");
    tr = nb_traceroute_new();
    if ( NULL == tr ) {
        /* Cannot create traceroute measurement instance */
        fprintf(stderr, "Cannot prepare traceroute measurement.\n");
        return EXIT_FAILURE;
    }

    /* Set a callback function */
    (void)nb_traceroute_set_callback(tr, cb_traceroute, NULL);

    /* Execute traceroute (IPv4) */
    ret = nb_traceroute_exec(tr, "netsurvey.jar.jp", AF_INET, 32, 5.0);
    if ( 0 != ret ) {
        /* Cannot execute the traceroute measurement */
        fprintf(stderr, "Cannot execute traceroute measurement (IPv4).\n");
        return EXIT_FAILURE;
    }

    /* Execute traceroute (IPv6) */
    ret = nb_traceroute_exec(tr, "netsurvey.jar.jp", AF_INET6, 32, 5.0);
    if ( 0 != ret ) {
        /* Cannot execute the traceroute measurement */
        fprintf(stderr, "Cannot execute traceroute measurement (IPv6).\n");
        return EXIT_FAILURE;
    }

    nb_traceroute_delete(tr);
    printf("Finishied traceroute.\n");

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
