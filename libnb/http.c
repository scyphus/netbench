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
                         void *user)
{
    /* Set the callback function */
    obj->cb = cbfunc;
    obj->user = user;

    return 0;
}

/*
 * Set a callback function
 */
int
nb_http_post_set_callback(nb_http_post_t *obj, nb_http_post_cb_f cbfunc,
                          void *user)
{
    /* Set the callback function */
    obj->cb = cbfunc;
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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
