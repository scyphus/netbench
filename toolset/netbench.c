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

#define API_SERVER "api.nb14.jar.jp"
#define IPV4_SERVER "v4.nb14.jar.jp"
#define IPV6_SERVER "v6.nb14.jar.jp"

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
void
cb_hget(nb_http_get_t *hget, off_t hlen, off_t clen, double t0, double cur,
        off_t tx, off_t rx)
{
    printf("Downloaded %.2lf %% (%.2lf sec)\n", 100.0 * rx / (hlen + clen),
           cur - t0);
}
void
cb_hpost(nb_http_post_t *hpost, off_t hlen, off_t clen, double t0, double cur,
         off_t btx, off_t tx, off_t rx)
{
    printf("Uploaded %.2lf %% (%.2lf sec)\n", 100.0 * btx / (hlen + clen),
           cur - t0);
}

int
main(int argc, const char *const argv[])
{
    int ret;
    nb_ping_t *ping;
    nb_traceroute_t *tr;
    nb_http_get_t *hget;
    nb_http_post_t *hpost;

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
    ret = nb_ping_exec(ping, IPV4_SERVER, 12, 10, 1.0, 3.0);
    if ( 0 != ret ) {
        /* Cannot execute the ping measurement */
        fprintf(stderr, "Cannot execute ping measurement.\n");
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
    ret = nb_ping_exec(ping, IPV6_SERVER, 2, 10, 1.0, 3.0);
    if ( 0 != ret ) {
        /* Cannot execute the ping measurement */
        fprintf(stderr, "Cannot execute ping measurement.\n");
    }

    nb_ping_close(ping);
    printf("Finishied ping (IPv6).\n");


    /* Prepare the traceroute measurement */
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
    ret = nb_traceroute_exec(tr, IPV4_SERVER, AF_INET, 32, 5.0);
    if ( 0 != ret ) {
        /* Cannot execute the traceroute measurement */
        fprintf(stderr, "Cannot execute traceroute measurement (IPv4).\n");
    }

    /* Execute traceroute (IPv6) */
    ret = nb_traceroute_exec(tr, IPV6_SERVER, AF_INET6, 32, 5.0);
    if ( 0 != ret ) {
        /* Cannot execute the traceroute measurement */
        fprintf(stderr, "Cannot execute traceroute measurement (IPv6).\n");
    }

    nb_traceroute_delete(tr);
    printf("Finishied traceroute.\n");


    /* Prepare the HTTP (GET) measurement */
    printf("Starting HTTP download ...\n");
    hget = nb_http_get_new("TESTID");
    if ( NULL == hget ) {
        /* Cannot create HTTP (GET) measurement instance */
        fprintf(stderr, "Cannot prepare HTTP (GET) measurement.\n");
        return EXIT_FAILURE;
    }

    /* Set a callback function */
    (void)nb_http_get_set_callback(hget, cb_hget, 0.5, NULL);

    /* Execute HTTP download (IPv4) */
    ret = nb_http_get_exec(hget, "http://"IPV4_SERVER"/scr/download.php",
                           AF_INET, 5.0);
    if ( 0 != ret ) {
        /* Cannot execute the HTTP (GET) measurement */
        fprintf(stderr, "Cannot execute HTTP (GET) measurement (IPv4).\n");
    }

    /* Execute HTTP download (IPv6) */
    ret = nb_http_get_exec(hget, "http://"IPV6_SERVER"/scr/download.php",
                           AF_INET6, 5.0);
    if ( 0 != ret ) {
        /* Cannot execute the HTTP (GET) measurement */
        fprintf(stderr, "Cannot execute HTTP (GET) measurement (IPv6).\n");
    }

    nb_http_get_delete(hget);
    printf("Finishied HTTP download.\n");


    /* Prepare the HTTP (POST) measurement */
    printf("Starting HTTP upload ...\n");
    hpost = nb_http_post_new("TESTID");
    if ( NULL == hpost ) {
        /* Cannot create HTTP (POST) measurement instance */
        fprintf(stderr, "Cannot prepare HTTP (POST) measurement.\n");
        return EXIT_FAILURE;
    }

    /* Set a callback function */
    (void)nb_http_post_set_callback(hpost, cb_hpost, 0.5, NULL);

    /* Execute HTTP upload (IPv4) */
    ret = nb_http_post_exec(hpost, "http://"IPV4_SERVER"/scr/upload.php",
                            AF_INET, 100 * 1000 * 1000, 5.0);
    if ( 0 != ret ) {
        /* Cannot execute the HTTP (POST) measurement */
        fprintf(stderr, "Cannot execute HTTP (POST) measurement (IPv4).\n");
    }

    /* Execute HTTP upload (IPv6) */
    ret = nb_http_post_exec(hpost, "http://"IPV6_SERVER"/scr/upload.php",
                            AF_INET6, 10 * 1000 * 1000, 5.0);
    if ( 0 != ret ) {
        /* Cannot execute the HTTP (POST) measurement */
        fprintf(stderr, "Cannot execute HTTP (POST) measurement (IPv6).\n");
    }

    nb_http_post_delete(hpost);
    printf("Finishied HTTP upload.\n");

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
