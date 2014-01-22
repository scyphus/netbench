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

struct ip_hdr {
    uint8_t verlen;
    uint8_t diffserv;
    uint16_t total_len;
    uint16_t ident;
    uint16_t offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t headerChecksum;
    uint8_t sourceAddress[4];
    uint8_t destinationAddress[4];
    // options...
    // data...
} __attribute__ ((packed));

struct icmp_hdr {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t ident;
    uint16_t seq;
    // data...
} __attribute__ ((packed));

struct icmp6_hdr {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    // data...
} __attribute__ ((packed));

/*
 * Send an ICMP echo request
 */
static int
_ping_send(nb_ping_t *obj, struct addrinfo *dai, uint16_t ident, uint16_t seq,
           size_t sz, double *tm)
{
    ssize_t ret;
    uint8_t buf[BUFFER_SIZE];
    struct icmp_hdr *icmp;
    size_t pktsize;
    size_t i;

    /* Compute packet size */
    pktsize = sizeof(struct icmp_hdr) + sz;
    if ( pktsize > BUFFER_SIZE ) {
        return -1;
    }

    /* Build an ICMP packet */
    icmp = (struct icmp_hdr *)buf;
    icmp->type = ICMP_TYPE_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->ident = htons(ident);
    icmp->seq = htons(seq);

    /* Fill with values */
    for ( i = sizeof(struct icmp_hdr); i < pktsize; i++ ) {
        buf[i] = i % 0xff;
    }
    icmp->checksum = nb_checksum(buf, pktsize);

    *tm = nb_microtime();
    ret = sendto(obj->sock, buf, pktsize, 0, (struct sockaddr *)dai->ai_addr,
                 dai->ai_addrlen);
    if ( ret < 0 ) {
        /* Failed to send the packet */
        return -1;
    }

    return 0;
}

/*
 * Receive an ICMP echo reply
 */
static int
_ping_recv(nb_ping_t *obj, struct addrinfo *dai, uint16_t *seq, double *tm,
           nb_ping_result_t *res)
{
    uint8_t buf[BUFFER_SIZE];
    struct sockaddr_storage saddr;
    socklen_t saddrlen;
    ssize_t nr;
    struct icmp_hdr *ricmp;
    struct ip_hdr *rip;
    int iphdrlen;
    struct sockaddr_in *sin4;
    struct sockaddr_in6 *sin6;

    saddrlen = sizeof(saddr);
    nr = recvfrom(obj->sock, buf, BUFFER_SIZE, 0, (struct sockaddr *)&saddr,
                  &saddrlen);
    *tm = nb_microtime();
    if ( nr < 0 ) {
        /* Read nothing */
        return -1;
    }

    /* Check the source address */
    if ( dai->ai_family != saddr.ss_family ) {
        return -1;
    }
    if ( AF_INET == dai->ai_family ) {
        sin4 = (struct sockaddr_in *)dai->ai_addr;
        if ( ((struct sockaddr_in *)&saddr)->sin_addr.s_addr
             != sin4->sin_addr.s_addr ) {
            return -1;
        }
    } else if ( AF_INET6 == dai->ai_family ) {
        sin6 = (struct sockaddr_in6 *)dai->ai_addr;
        if ( 0 != memcmp(((struct sockaddr_in6 *)&saddr)->sin6_addr.s6_addr,
                         sin6->sin6_addr.s6_addr,
                         sizeof(sin6->sin6_addr.s6_addr)) ) {
        }
    } else {
        return -1;
    }

    /* Check it */
    if ( nr < sizeof(struct ip_hdr) ) {
        return -1;
    }

    /* IP header */
    rip = (struct ip_hdr *)buf;
    iphdrlen = rip->verlen & 0xf;
    if ( nr < 4 * iphdrlen + sizeof(struct icmp_hdr) ) {
        return -1;
    }
    /* Skip IP header */
    ricmp = (struct icmp_hdr *)(buf + 4 * iphdrlen);
    if ( ICMP_TYPE_ECHO_REPLY != ricmp->type || 0 != ricmp->code ) {
        /* Error */
        return -1;
    }
    *seq = ntohs(ricmp->seq);
    if ( *seq >= res->cnt ) {
        /* Invalid sequence */
        return -1;
    }
    if ( ntohs(ricmp->ident) != res->items[*seq].ident ) {
        /* Invalid identifier */
        return -1;
    }

    return 0;
}



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
 * Send a ping packet and receive its reply
 */
int
nb_ping_exec(nb_ping_t *obj, const char *target, size_t sz, int n,
             double interval, double timeout)
{
    struct addrinfo *ai;
    struct addrinfo hints;
    struct addrinfo *ressave;
    int i;
    int err;
    int done;
    int nsent;
    int nrecv;
    double t0;
    double t1;
    double tm;
    uint16_t ident;
    uint16_t seq;
    struct pollfd fds[1];
    int events;
    double gto;
    nb_ping_result_t *res;

    /* Setup ai and hints to get address info */
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = obj->family;
    hints.ai_socktype = SOCK_DGRAM;
    err = getaddrinfo(target, NULL, &hints, &ressave);
    if ( 0 != err ) {
        /* Cannot resolve the target host */
        return -1;
    }

    /* Save the first addrinfo */
    if ( NULL == ressave ) {
        /* Error if the first addrinfo is NULL */
        freeaddrinfo(ressave);
        return -1;
    }
    ai = ressave;

    /* Allocate for the results */
    res = malloc(sizeof(nb_ping_result_t));
    if ( NULL == res ) {
        /* Free the returned addrinfo */
        freeaddrinfo(ressave);
        return -1;
    }
    res->cnt = n;
    res->items = malloc(sizeof(nb_ping_result_item_t) * res->cnt);
    if ( NULL == res->items ) {
        free(res);
        /* Free the returned addrinfo */
        freeaddrinfo(ressave);
        return -1;
    }
    for ( i = 0; i < n; i++ ){
        res->items[i].stat = -1;
    }

    /* Obtain the started time */
    t0 = nb_microtime();

    done = 0;
    nsent = 0;
    nrecv = 0;
    while ( !obj->cancel && !done ) {
        /* Obtain the current time */
        t1 = nb_microtime();

        if ( nsent < n && interval * nsent < t1 - t0 ) {
            /* FIXME? */
            ident = random();
            err = _ping_send(obj, ai, ident, nsent, sz, &tm);
            if ( 0 != err ) {
                obj->cancel = 1;
                continue;
            }

            /* Set the result item */
            res->items[nsent].stat = 0;
            res->items[nsent].ident = ident;
            res->items[nsent].sent = tm;
            nsent++;
        }

        /* Update the current time */
        t1 = nb_microtime();

        /* Calculate the timeout for polling */
        if ( nsent < n ) {
            /* Check the timeout to the next */
            gto = interval * nsent - (t1 - t0);
            if ( gto <= 0.0 ) {
                /* Send again */
                continue;
            }
        } else {
            gto = interval * nsent - (t1 - t0) + timeout;
            if ( gto <= 0.0 ) {
                gto = 0.0;
            }
        }

        /* Poll */
        fds[0].fd = obj->sock;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        events = poll(fds, 1, (int)(gto * 1000));

        if ( events < 0 ) {
            if ( EINTR == errno ) {
                /* Interrupt */
                continue;
            } else {
                /* Other errors */
                obj->cancel = 1;
                continue;
            }
        } else if ( 0 == events ) {
            /* Timeout */
            t1 = nb_microtime();
            if ( interval * n + timeout < (t1 - t0) ) {
                /* Reached the timeout */
                done = 1;
            }
            continue;
        } else {
            if ( fds[0].revents & (POLLERR | POLLHUP | POLLNVAL) ) {
                /* Error */
                obj->cancel = 1;
                continue;
            } else {
                if ( fds[0].revents & POLLIN ) {
                    /* Received */
                    err = _ping_recv(obj, ai, &seq, &tm, res);
                    if ( 0 != err ) {
                        continue;
                    }

                    if ( NULL != obj->cb ) {
                        obj->cb(obj, seq, tm - res->items[seq].sent);
                    }
                    res->items[seq].stat++;
                    res->items[seq].recv = tm;
                    nrecv++;
                    if ( nrecv >= n ) {
                        done = 1;
                    }
                }
            }
        }
    }

    /* Free the result */
    if ( NULL != obj->last_results ) {
        free(obj->last_results->items);
        free(obj->last_results);
    }
    obj->last_results = res;

    /* Free the returned addrinfo */
    freeaddrinfo(ressave);

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
