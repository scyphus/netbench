/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#ifndef _NETBENCH_H
#define _NETBENCH_H

#include <stdint.h>
#include <netdb.h>

/*
 * Measurement results of ping
 */
typedef struct _ping_result_item {
    int stat;
    uint16_t ident;
    double sent;
    double recv;
} nb_ping_result_item_t;
typedef struct _ping_result {
    size_t cnt;
    nb_ping_result_item_t *items;
} nb_ping_result_t;

typedef struct _ping nb_ping_t;

/* Callback function */
typedef void (*nb_ping_cb_f)(nb_ping_t *, int, double);

struct _ping {
    int sock;
    int family;
    nb_ping_cb_f cb;
    void *user;
    nb_ping_result_t *last_results;
    int cancel;
};

/*
 * Traceroute
 */
typedef struct _traceroute_result_item {
    int stat;
    int ttl;
    /*uint16_t sport;*/
    double sent;
    double recv;
    struct sockaddr_storage saddr;
    socklen_t saddrlen;
} nb_traceroute_result_item_t;
typedef struct _traceroute_result {
    size_t cnt;
    size_t cntres;
    nb_traceroute_result_item_t *items;
} nb_traceroute_result_t;
typedef struct _traceroute nb_traceroute_t;
typedef void (*nb_traceroute_cb_f)(nb_traceroute_t *, int,
                                   const struct sockaddr_storage *, socklen_t,
                                   double);
struct _traceroute {
    nb_traceroute_cb_f cb;
    void *user;
    nb_traceroute_result_t *last_results;
    int cancel;
};

/*
 * HTTP (GET)
 */
typedef struct _http_get_result_item {
    double tm;
    off_t tx;
    off_t rx;
    off_t brx;
} nb_http_get_result_item_t;
typedef struct _http_get_result {
    size_t cnt;
    size_t cntres;
    nb_http_get_result_item_t *items;
    size_t hlen;
    size_t clen;
} nb_http_get_result_t;
typedef struct _http_get nb_http_get_t;
typedef void (*nb_http_get_cb_f)(nb_http_get_t *, size_t, size_t, double,
                                 size_t, double);
struct _http_get {
    nb_http_get_cb_f cb;
    void *user;
    char *mid;
    nb_http_get_result_t *last_result;
    int cancel;
};

/*
 * HTTP (POST)
 */
typedef struct _http_post_result_item {
    double tm;
    size_t tx;
    size_t rx;
    size_t btx;                 /* Buffered TX */
} nb_http_post_result_item_t;
typedef struct _http_post_result {
    size_t cnt;
    size_t cntres;
    nb_http_post_result_item_t *items;
    size_t hlen;
    size_t clen;
} nb_http_post_result_t;
typedef struct _http_post nb_http_post_t;
typedef void (*nb_http_post_cb_f)(nb_http_post_t *, size_t, size_t, double,
                                  size_t, double);
struct _http_post {
    nb_http_post_cb_f cb;
    char *mid;
    void *user;
    nb_http_post_result_t *last_result;
    int cancel;
};




#ifdef __cplusplus
extern "C" {
#endif

    /* Generic functions */
    double nb_microtime(void);
    uint16_t nb_checksum(const uint8_t *, size_t);

    /* Ping */
    nb_ping_t * nb_ping_open(int);
    int nb_ping_set_callback(nb_ping_t *, nb_ping_cb_f, void *);
    int nb_ping_exec(nb_ping_t *, const char *, size_t, int, double, double);
    void nb_ping_close(nb_ping_t *);

    /* Traceroute */
    nb_traceroute_t * nb_traceroute_new(void);
    int
    nb_traceroute_set_callback(nb_traceroute_t *, nb_traceroute_cb_f, void *);
    int nb_traceroute_exec(nb_traceroute_t *, const char *, int, int, double);
    void nb_traceroute_delete(nb_traceroute_t *);

    /* HTTP (GET) */
    nb_http_get_t * nb_http_get_new(const char *);
    int nb_http_get_set_callback(nb_http_get_t *, nb_http_get_cb_f, void *);
    int nb_http_get_exec(nb_http_get_t *, const char *, double);
    void nb_http_get_delete(nb_http_get_t *);

    /* HTTP (POST) */
    nb_http_post_t * nb_http_post_new(const char *);
    int nb_http_post_set_callback(nb_http_post_t *, nb_http_post_cb_f, void *);
    int nb_http_post_exec(nb_http_post_t *, const char *, size_t, double);
    void nb_http_post_delete(nb_http_post_t *);


#ifdef __cplusplus
}
#endif

#endif /* _NETBENCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
