/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include "config.h"
#include "netbench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#define BUFFER_SIZE                     65536
#define ICMP_TYPE_ECHO_REQUEST          8
#define ICMP_TYPE_ECHO_REPLY            0
#define ICMPV6_TYPE_ECHO_REQUEST        128
#define ICMPV6_TYPE_ECHO_REPLY          129

/*
 * Open a ping4 socket
 */
nb_ping_t *
nb_ping_open(int family)
{
    nb_ping_t *obj;

    /* Allocate memory for the ping4 object */
    obj = malloc(sizeof(nb_ping_t));
    if ( NULL == obj ) {
        return NULL;
    }

    /* Open the socket */
    switch ( family ) {
    case AF_INET:
#if TARGET_FREEBSD || TARGET_NETBSD || TARGET_LINUX
        obj->sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
#else
        obj->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
#endif
        break;
    case AF_INET6:
#if TARGET_FREEBSD || TARGET_NETBSD || TARGET_LINUX
        obj->sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
#else
        obj->sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
#endif
        break;
    default:
        /* Invalid family */
        free(obj);
        return NULL;
    }
    if ( obj->sock < 0 ) {
        /* Cannot open the ICMP socket */
        free(obj);
        return NULL;
    }
    obj->family = family;
    obj->cb = NULL;
    obj->last_results = NULL;
    obj->cancel = 0;

    return obj;
}

/*
 * Set a callback function
 */
int
nb_ping_set_callback(nb_ping_t *obj, nb_ping_cb_f cbfunc, void *user)
{
    /* Set the callback function */
    obj->cb = cbfunc;
    obj->user = user;

    return 0;
}

/*
 * Close the ping socket
 */
void
nb_ping_close(nb_ping_t *obj)
{
    (void)close(obj->sock);
    if ( NULL != obj->last_results ) {
        free(obj->last_results->items);
        free(obj->last_results);
    }
    free(obj);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
