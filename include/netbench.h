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
