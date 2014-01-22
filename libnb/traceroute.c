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

#define BUFFER_SIZE                     65536
#define TRACEROUTE_USLEEP               1000

struct icmp_hdr {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t pad;
    // data...
} __attribute__ ((packed));

/*
 * Open a traceroute socket
 */
nb_traceroute_t *
nb_traceroute_new(void)
{
    nb_traceroute_t *obj;

    /* Allocate memory for the ping4 object */
    obj = malloc(sizeof(nb_traceroute_t));
    if ( NULL == obj ) {
        return NULL;
    }
    obj->cb = NULL;
    obj->last_results = NULL;
    obj->cancel = 0;

    return obj;
}

/*
 * Set a callback function
 */
int
nb_traceroute_set_callback(nb_traceroute_t *obj, nb_traceroute_cb_f cbfunc,
                           void *user)
{
    /* Set the callback function */
    obj->cb = cbfunc;
    obj->user = user;

    return 0;
}

/*
 * Receive an ICMP time exceeded (FIXME)
 */
static int
_icmp4_recv(int sock, struct sockaddr_storage *saddr, socklen_t *saddrlen,
            double *t0)
{
    uint8_t buf[BUFFER_SIZE];
    ssize_t nr;

    *saddrlen = sizeof(struct sockaddr_storage);
    nr = recvfrom(sock, buf, BUFFER_SIZE, 0, (struct sockaddr *)saddr,
                  saddrlen);
    *t0 = nb_microtime();
    if ( nr < 0 ) {
        /* Read nothing */
        return -1;
    }
    if ( nr < 24 ) {
        return -1;
    }
    if ( 0x45 != buf[0] ) {
        /* IP */
        return -1;
    }
    if ( 0x01 != buf[9] ) {
        /* Must be ICMP */
        return -1;
    }
    if ( 0x0b != buf[20] && 0x03 != buf[20] ) {
        /* Must be time exceed or destination unreachable */
        return -1;
    }

    /* Check the source address */
    if ( AF_INET != saddr->ss_family ) {
        return -1;
    }

    return 0;
}
static int
_icmp6_recv(int sock, struct sockaddr_storage *saddr, socklen_t *saddrlen,
            double *t0)
{
    uint8_t buf[BUFFER_SIZE];
    ssize_t nr;

    nr = recvfrom(sock, buf, BUFFER_SIZE, 0, (struct sockaddr *)saddr,
                  saddrlen);
    *t0 = nb_microtime();
    if ( nr < 0 ) {
        /* Read nothing */
        return -1;
    }
    if ( nr < 24 ) {
        return -1;
    }
    if ( 0x01 != buf[0] && 0x03 != buf[0] ) {
        /* Must be Destination unreachable or Time exceeded */
        return -1;
    }

    /* Check the source address */
    if ( AF_INET6 != saddr->ss_family ) {
        return -1;
    }

    return 0;
}

/*
 * Execute traceroute
 */
int
nb_traceroute_exec(nb_traceroute_t *obj, const char *target, int family,
                   int maxttl, double timeout)
{
    int sock;
    int icmpsock;
    int error;
    int ttl;
    int opt;
    int i;
    double t0;
    double t1;
    struct addrinfo ai;
    struct addrinfo hints, *ai0, *ressave;
    int pktsize = 40;
    const char *service = "33434";
    struct timeval recv_tv;
    uint8_t buf[BUFFER_SIZE];
    ssize_t sz;
    int ret;
    struct sockaddr_storage saddr;
    socklen_t saddrlen;
    nb_traceroute_result_t *result;

    /* Open an ICMP socket */
    if ( AF_INET == family ) {
        icmpsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    } else if ( AF_INET6 == family ) {
        icmpsock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
    } else {
        return -1;
    }
    if ( icmpsock < 0 ) {
        /* Cannot open ICMP socket */
        return -1;
    }

    /* Set timeout */
    recv_tv.tv_sec = (int)timeout;
    recv_tv.tv_usec = (int)((timeout - (int)timeout) * 1000000);
    setsockopt(icmpsock, SOL_SOCKET, SO_RCVTIMEO, &recv_tv, sizeof(recv_tv));

    /* Prepare buffer */
    for ( i = 0; i < pktsize; i++ ) {
        buf[i] = i & 0xff;
    }

    /* Open an UDP socket */
    sock = -1;
    bzero(&ai, sizeof(struct addrinfo));
    /* Setup hints to get address info for a UDP socket */
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = family;
    hints.ai_socktype = SOCK_DGRAM;
    /* Obtain address info of the target */
    error = getaddrinfo(target, service, &hints, &ressave);
    if ( 0 != error ) {
        /* Error on getaddrinfo */
        close(icmpsock);
        return -1;
    }
    /* Search and open the first available socket */
    sock = -1;
    for ( ai0 = ressave; NULL != ai0; ai0 = ai0->ai_next ) {
        /* Open a socket */
        sock = socket(ai0->ai_family, ai0->ai_socktype, ai0->ai_protocol);
        if ( sock < 0 ) {
            /* Ignore unavailable sockets */
            continue;
        }
        /* Succeed */
        memcpy(&ai, ai0, sizeof(struct addrinfo));
        break;
    }
    /* Check the socket */
    if( sock < 0 ){
        freeaddrinfo(ressave);
        close(icmpsock);
        return -1;
    }

    /* Allocate for the results */
    result = malloc(sizeof(nb_traceroute_result_t));
    if ( NULL == result ) {
        return -1;
    }
    result->cnt = 0;
    result->cntres = maxttl + 1;
    result->items = malloc(sizeof(nb_traceroute_result_item_t) * result->cntres);
    if ( NULL == result->items ) {
        free(result);
        return -1;
    }

    /* For each TTL */
    for ( ttl = 1; ttl <= maxttl; ttl++ ) {
        if ( obj->cancel ) {
            break;
        }

        result->cnt = ttl;
        result->items[ttl - 1].ttl = ttl;
        result->items[ttl - 1].stat = -1;

        opt = ttl;
        if ( AF_INET6 == family ) {
            error = setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &opt,
                               sizeof(int));
        } else {
            error = setsockopt(sock, IPPROTO_IP, IP_TTL, &opt, sizeof(int));
        }
        if ( 0 != error ) {
            printf(" *");
            continue;
        }
        usleep(TRACEROUTE_USLEEP);

        t0 = nb_microtime();
        sz = sendto(sock, buf, pktsize, 0, ai.ai_addr, ai.ai_addrlen);
        if ( sz < 0 ) {
            continue;
        }
        result->items[ttl - 1].stat = 0;
        result->items[ttl - 1].sent = t0;

        if ( AF_INET == family ) {
            ret = _icmp4_recv(icmpsock, &saddr, &saddrlen, &t1);
            if ( ret < 0 ) {
                continue;
            }
            (void)memcpy(&result->items[ttl - 1].saddr, &saddr,
                         sizeof(struct sockaddr_storage));
            if ( NULL != obj->cb ) {
                obj->cb(obj, ttl, &saddr, saddrlen,
                        t1 - result->items[ttl - 1].sent);
            }
            result->items[ttl - 1].stat++;
            result->items[ttl - 1].recv = t1;
            /* Check destination */
            if ( ((struct sockaddr_in *)ai.ai_addr)->sin_addr.s_addr
                 == ((struct sockaddr_in *)&saddr)->sin_addr.s_addr ) {
                break;
            }
        } else if ( AF_INET6 == family ) {
            ret = _icmp6_recv(icmpsock, &saddr, &saddrlen, &t1);
            if ( ret < 0 ) {
                continue;
            }
            (void)memcpy(&result->items[ttl - 1].saddr, &saddr,
                         sizeof(struct sockaddr_storage));
            if ( NULL != obj->cb ) {
                obj->cb(obj, ttl, &saddr, saddrlen,
                        t1 - result->items[ttl - 1].sent);
            }
            result->items[ttl - 1].stat++;
            result->items[ttl - 1].recv = t1;
            /* Check destination */
            if ( memcmp(((struct sockaddr_in6 *)ai.ai_addr)->sin6_addr.s6_addr,
                        ((struct sockaddr_in6 *)&saddr)->sin6_addr.s6_addr,
                        16) == 0 ) {
                break;
            }
        }
    }

    /* Close */
    freeaddrinfo(ressave);
    close(icmpsock);
    close(sock);

    /* Free the result */
    if ( NULL != obj->last_results ) {
        free(obj->last_results);
    }
    obj->last_results = result;

    return 0;
}

/*
 * Close the traceroute socket
 */
void
nb_traceroute_delete(nb_traceroute_t *obj)
{
    if ( NULL != obj->last_results ) {
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
