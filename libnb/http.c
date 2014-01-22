/*_
 * Copyright 2010-2011,2013-2014 Scyphus Solutions Co. Ltd.
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include "netbench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
/* Socket */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>

#define HTTP_SOCKET_TIMEOUT             30.0
#define RESULT_ITEMS_RESERVE_UNIT       4096
#define UPLOAD_WAIT_USLEEP              1000
#define BUFFER_SIZE                     65536


/*
 * Create new http_get instance
 */
nb_http_get_t *
nb_http_get_new(const char *mid)
{
    nb_http_get_t *obj;

    obj = malloc(sizeof(nb_http_get_t));
    if ( NULL == obj ) {
        return NULL;
    }
    obj->mid = strdup(mid);
    if ( NULL == obj->mid ) {
        free(obj);
        return NULL;
    }

    obj->cb = NULL;
    obj->last_result = NULL;
    obj->cancel = 0;

    return obj;
}

/*
 * Create new http_post instance
 */
nb_http_post_t *
nb_http_post_new(const char *mid)
{
    nb_http_post_t *obj;

    obj = malloc(sizeof(nb_http_post_t));
    if ( NULL == obj ) {
        return NULL;
    }
    obj->mid = strdup(mid);
    if ( NULL == obj->mid ) {
        free(obj);
        return NULL;
    }

    obj->cb = NULL;
    obj->last_result = NULL;
    obj->cancel = 0;

    return obj;
}

/*
 * Set a callback function
 */
int
nb_http_get_set_callback(nb_http_get_t *obj, nb_http_get_cb_f cbfunc,
                         double cbfreq, void *user)
{
    /* Set the callback function */
    obj->cb = cbfunc;
    obj->cbfreq = cbfreq;
    obj->user = user;

    return 0;
}

/*
 * Set a callback function
 */
int
nb_http_post_set_callback(nb_http_post_t *obj, nb_http_post_cb_f cbfunc,
                          double cbfreq, void *user)
{
    /* Set the callback function */
    obj->cb = cbfunc;
    obj->cbfreq = cbfreq;
    obj->user = user;

    return 0;
}

/*
 * Delete an http_get instance
 */
void
nb_http_get_delete(nb_http_get_t *obj)
{
    if ( NULL != obj->last_result ) {
        free(obj->last_result->items);
        free(obj->last_result);
    }
    free(obj->mid);
    free(obj);
}

/*
 * Delete an http_post instance
 */
void
nb_http_post_delete(nb_http_post_t *obj)
{
    if ( NULL != obj->last_result ) {
        free(obj->last_result->items);
        free(obj->last_result);
    }
    free(obj->mid);
    free(obj);
}



/*
 * Open a TCP socket
 */
static int
_open_stream_socket(const char *host, const char *service, int family)
{
    int sock;
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *ressave;
    int err;

    /* Open a socket */
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    err = getaddrinfo(host, service, &hints, &res);
    if ( 0 != err ) {
        /* Error */
        return -1;
    }

    /* Get first connection */
    ressave = res;
    sock = -1;
    do {
        sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if ( sock < 0 ) {
            /* ignore a connection error. */
            continue;
        }
        err = connect(sock, res->ai_addr, res->ai_addrlen);
        if ( 0 != err ) {
            /* Error */
            (void)close(sock);
            sock = -1;
        } else {
            /* Succeed */
            break;
        }
    } while ( NULL != (res = res->ai_next) );

    if ( sock < 0 ) {
        /* No socket found */
        freeaddrinfo(ressave);
        return -1;
    }

    return sock;
}

/*
 * Read the response header
 * Note that a part of response body is possibly read due to the buffer size
 */
static int
_read_response_header(int sock, char **hdrstr, off_t *hdrlen, char **bdystr,
                      off_t *bdylen)
{
    /* Buffer */
    char buf[4096];
    /* Counters */
    ssize_t i;
    ssize_t nr;
    ssize_t n;
    int nl;
    /* A flag of end-of-header */
    int eoh;
    /* Temporary string */
    char *tmpstr;

    /* Read until the end of the response header */
    eoh = 0;
    nl = 0;
    nr = 0;
    *hdrlen = 0;
    *hdrstr = NULL;
    while ( !eoh ) {
        /* Receive */
        n = read(sock, buf, sizeof(buf));
        if ( n <= 0 ) {
            /* Cannot read any more response */
            if ( NULL != *hdrstr ) {
                free(*hdrstr);
            }
            return -ETIMEDOUT;
        }
        /* Search the end-of-header */
        for ( i = 0; i < n; i++ ) {
            if ( '\n' == buf[i] ) {
                nl++;
            } else if ( '\r' == buf[i] && i + 1 < n && '\n' == buf[i+1] ) {
                i++;
                nl++;
            } else {
                nl = 0;
            }
            if ( 2 == nl ) {
                /* End-of-header */
                eoh = 1;
                i++;
                break;
            }
        }
        *hdrlen += i;

        /* Extend the response header buffer */
        tmpstr = realloc(*hdrstr, sizeof(char) * (size_t)(*hdrlen + 1));
        if ( NULL == tmpstr ) {
            /* Memory error */
            if ( NULL != *hdrstr ) {
                free(*hdrstr);
            }
            return -ENOMEM;
        }
        *hdrstr = tmpstr;
        (void)memcpy((*hdrstr) + nr, buf, i);
        /* Convert asciz to reduce buffer overflow risk */
        (*hdrstr)[*hdrlen] = '\0';

        nr += n;
    }

    /* Read body */
    if ( nr > *hdrlen ) {
        /* Compute the loaded body length */
        *bdylen = nr - *hdrlen;
        /* +1 for null termination for safety */
        *bdystr = malloc(sizeof(char) * (size_t)(*bdylen + 1));
        if ( NULL ==  *bdystr ) {
            if ( NULL != *hdrstr ) {
                free(*hdrstr);
            }
            return -ENOMEM;
        }
        (void)memcpy(*bdystr, buf + i, (size_t)*bdylen);
        (*bdystr)[*bdylen] = '\0';
    } else {
        *bdystr = NULL;
        *bdylen = 0;
    }

    return 0;
}

/*
 * Build Request-URI corresponding the input parsed URL
 */
static char *
_build_request_uri(nb_parsed_url_t *purl)
{
    size_t len1;
    size_t len2;
    char *str1;
    char *str2;
    char *rurl;

    /* Get path */
    if ( NULL != purl->path ) {
        /* Get length */
        len1 = strlen(purl->path);
        str1 = purl->path;
    } else {
        len1 = 0;
        str1 = "";
    }

    /* Get query */
    if ( NULL != purl->query ) {
        /* Get length */
        len2 = strlen(purl->query);
        str2 = purl->query;
    } else {
        str2 = NULL;
    }

    if ( NULL != str2 ) {
        /* Allocate request url */
        rurl = malloc(sizeof(char) * (len1 + len2 + 3));
        if ( NULL == rurl ) {
            return NULL;
        }

        /* Copy */
        rurl[0] = '/';
        (void)memcpy(rurl+1, str1, len1);
        rurl[len1+1] = '?';
        (void)memcpy(rurl+len1+2, str2, len2);
        rurl[len1+len2+2] = '\0';
    } else {
        /* Allocate request url */
        rurl = malloc(sizeof(char) * (len1 + 2));
        if ( NULL == rurl ) {
            return NULL;
        }

        /* Copy */
        rurl[0] = '/';
        (void)memcpy(rurl+1, str1, len1);
        rurl[len1+1] = '\0';
    }

    return rurl;
}


/*
 * Execute a file download via HTTP
 */
int
nb_http_get_exec(nb_http_get_t *obj, const char *url, int family,
                 double duration)
{
    nb_parsed_url_t *purl;
    nb_http_get_result_t *result;
    nb_http_get_result_item_t *items;
    size_t cntres;
    int sock;
    int err;
    char *port;
    char *path;
    struct timeval tv;
    char req[BUFFER_SIZE];
    char buf[BUFFER_SIZE];
    char *hdrstr;
    off_t hdrlen;
    char *bdystr;
    off_t bdylen;
    nb_http_header_t *hdr;
    ssize_t nw;
    ssize_t nr;
    double t0;
    double t1;
    double t2;
    double curtm;
    double prevtm;
    off_t tx;
    off_t rx;
    off_t clen;
    off_t tsize;

    /* Allocate for the results */
    result = malloc(sizeof(nb_http_get_result_t));
    if ( NULL == result ) {
        return -1;
    }
    result->cnt = 0;
    result->cntres = RESULT_ITEMS_RESERVE_UNIT;
    result->items = malloc(sizeof(nb_http_get_result_item_t) * result->cntres);
    if ( NULL == result->items ) {
        free(result);
        return -1;
    }
    result->hlen = 0;
    result->clen = 0;

    /* Parse the URL */
    purl = nb_parse_url(url);
    if ( NULL == purl ) {
        free(result->items);
        free(result);
        return -1;
    }
    /* Only the scheme "http" is supported. */
    if ( strcasecmp("http", purl->scheme) ) {
        nb_parsed_url_free(purl);
        free(result->items);
        free(result);
        return -1;
    }
    port = purl->port;
    if ( NULL == port ) {
        /* Set default port */
        port = "80";
    }

    /* Open a socket */
    sock = _open_stream_socket(purl->host, port, family);
    if ( sock < 0 ) {
        nb_parsed_url_free(purl);
        free(result->items);
        free(result);
        return -1;
    }

    /* Set timeout */
    tv.tv_sec = (time_t)HTTP_SOCKET_TIMEOUT;
    tv.tv_usec = (suseconds_t)((HTTP_SOCKET_TIMEOUT
                                - (time_t)HTTP_SOCKET_TIMEOUT) * 1000000);
    if ( 0 != setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv,
                         sizeof(struct timeval)) ) {
        /* Error */
        (void)close(sock);
        nb_parsed_url_free(purl);
        free(result->items);
        free(result);
        return -1;
    }

    /* Initialize the variables for saving statistics */
    tx = 0;
    rx = 0;

    /* Build and send a request */
    path = _build_request_uri(purl);
    if ( NULL ==  path ) {
        /* Error */
        (void)close(sock);
        nb_parsed_url_free(purl);
        free(result->items);
        free(result);
        return -1;
    }
    snprintf(req, sizeof(req), "GET %.1024s HTTP/1.1\r\n"
             "Host: %.1024s\r\n"
             "User-Agent: %s\r\n"
             "X-Measurement-Id: %.100s\r\n"
             "Connection: close\r\n\r\n", path, purl->host, USER_AGENT,
             obj->mid);
    free(path);

    /* Free the parsed url */
    nb_parsed_url_free(purl);

    /* Obtain the current time */
    t0 = nb_microtime();
    /* For result */
    result->items[result->cnt].tm = t0;
    result->items[result->cnt].tx = tx;
    result->items[result->cnt].rx = rx;
    result->cnt++;

    /* Send request header */
    nw = send(sock, req, strlen(req), 0);
    if ( nw != strlen(req) ) {
        close(sock);
        free(result->items);
        free(result);
        return -1;
    }
    tx += nw;

    /* Read the response header */
    err = _read_response_header(sock, &hdrstr, &hdrlen, &bdystr, &bdylen);
    if ( err < 0 ) {
        /* Error */
        close(sock);
        free(result->items);
        free(result);
        return -1;
    }
    t1 = nb_microtime();
    rx += hdrlen + bdylen;

    /* Parse the response header */
    hdr = nb_parse_http_header(hdrstr, (size_t)hdrlen);
    if ( NULL == hdr ) {
        /* Error */
        free(hdrstr);
        if ( NULL != bdystr ) {
            free(bdystr);
        }
        close(sock);
        free(result->items);
        free(result);
        return -1;
    }
    /* Free the response */
    free(hdrstr);
    if ( NULL != bdystr ) {
        free(bdystr);
    }

    /* Get content length */
    clen = nb_http_header_get_content_length(hdr);

    /* Free the HTTP header */
    nb_http_header_delete(hdr);

    /* For result */
    result->hlen = hdrlen;
    result->clen = clen;
    result->items[result->cnt].tm = t1;
    result->items[result->cnt].tx = tx;
    result->items[result->cnt].rx = rx;
    result->cnt++;

    /* Call a callback function */
    if ( NULL != obj->cb ) {
        obj->cb(obj, result->hlen, result->clen, t0, t1, tx, rx);
    }

    /* Set read length */
    tsize = bdylen;

    /* Download the body */
    prevtm = t1;
    while ( (nr = recv(sock, buf, sizeof(buf), 0)) > 0 ) {
        curtm = nb_microtime();

        /* Update the information */
        tsize += nr;
        rx += nr;

        /* Insert a result item */
        if ( result->cnt >= result->cntres ) {
            /* Realloc */
            cntres = result->cntres + RESULT_ITEMS_RESERVE_UNIT;
            items = realloc(result->items,
                            sizeof(nb_http_get_result_item_t) * cntres);
            if ( NULL != items ) {
                result->items = items;
                result->cntres = cntres;
                result->items[result->cnt].tm = curtm;
                result->items[result->cnt].tx = tx;
                result->items[result->cnt].rx = rx;
                result->cnt++;
            }
        } else {
            result->items[result->cnt].tm = curtm;
            result->items[result->cnt].tx = tx;
            result->items[result->cnt].rx = rx;
            result->cnt++;
        }

        /* Report by calling a callback function */
        if ( NULL != obj->cb ) {
            if ( curtm - prevtm >= obj->cbfreq ) {
                obj->cb(obj, result->hlen, result->clen, t0, curtm, tx, rx);
                prevtm = curtm;
            }
        }
        if ( curtm - t0 > duration ) {
            break;
        }
    }
    if ( curtm != prevtm ) {
        if ( NULL != obj->cb ) {
            obj->cb(obj, result->hlen, result->clen, t0, curtm, tx, rx);
        }
    }

    /* Completed time of the download */
    t2 = prevtm;

    /* Close the socket */
    shutdown(sock, SHUT_RDWR);
    (void)close(sock);

    /* Update the result */
    if ( NULL != obj->last_result ) {
        free(obj->last_result->items);
        free(obj->last_result);
    }
    obj->last_result = result;

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
