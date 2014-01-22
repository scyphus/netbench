/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include "netbench_private.h"
#include <netbench.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>

/* Prototype declarations */
static __inline__ int _is_scheme_char(int);

/*
 * Check whether the character is permitted in scheme string
 */
static __inline__ int
_is_scheme_char(int c)
{
    return (!isalpha(c) && '+' != c && '-' != c && '.' != c) ? 0 : 1;
}


/*
 * Get current time in microtime
 */
double
nb_microtime(void)
{
    struct timeval tv;
    double microsec;

    if ( 0 != gettimeofday(&tv, NULL) ) {
        return 0.0;
    }

    microsec = (double)tv.tv_sec + (1.0 * tv.tv_usec / 1000000);

    return microsec;
}

/*
 * Calculate checksum
 */
uint16_t
nb_checksum(const uint8_t *buf, size_t len) {
    size_t nleft;
    int32_t sum;
    const uint16_t *cur;
    union {
        uint16_t us;
        uint8_t uc[2];
    } last;
    uint16_t ret;

    nleft = len;
    sum = 0;
    cur = (const uint16_t *)buf;

    while ( nleft > 1 ) {
        sum += *cur;
        cur += 1;
        nleft -= 2;
    }

    if ( 1 == nleft ) {
        last.uc[0] = *(const uint8_t *)cur;
        last.uc[1] = 0;
        sum += last.us;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    ret = ~sum;

    return ret;
}


/*
 * See RFC 1738, 3986
 */
nb_parsed_url_t *
nb_parse_url(const char *url)
{
    nb_parsed_url_t *purl;
    const char *tmpstr;
    const char *curstr;
    long len;
    int i;
    int userpass_flag;
    int bracket_flag;

    /* Allocate the parsed url storage */
    purl = malloc(sizeof(nb_parsed_url_t));
    if ( NULL == purl ) {
        return NULL;
    }
    purl->scheme = NULL;
    purl->host = NULL;
    purl->port = NULL;
    purl->path = NULL;
    purl->query = NULL;
    purl->fragment = NULL;
    purl->username = NULL;
    purl->password = NULL;

    curstr = url;

    /*
     * <scheme>:<scheme-specific-part>
     * <scheme> := [a-z\+\-\.]+
     *             upper case = lower case for resiliency
     */
    /* Read scheme */
    tmpstr = strchr(curstr, ':');
    if ( NULL == tmpstr ) {
        /* Not found the character */
        nb_parsed_url_free(purl);
        return NULL;
    }
    /* Get the scheme length */
    len = tmpstr - curstr;
    /* Check restrictions */
    for ( i = 0; i < len; i++ ) {
        if ( !_is_scheme_char(curstr[i]) ) {
            /* Invalid format */
            nb_parsed_url_free(purl);
            return NULL;
        }
    }
    /* Copy the scheme to the storage */
    purl->scheme = malloc(sizeof(char) * (len + 1));
    if ( NULL == purl->scheme ) {
        nb_parsed_url_free(purl);
        return NULL;
    }
    (void)strncpy(purl->scheme, curstr, len);
    purl->scheme[len] = '\0';
    /* Make the character to lower if it is upper case. */
    for ( i = 0; i < len; i++ ) {
        purl->scheme[i] = tolower(purl->scheme[i]);
    }
    /* Skip ':' */
    tmpstr++;
    curstr = tmpstr;

    /*
     * //<user>:<password>@<host>:<port>/<url-path>
     * Any ":", "@" and "/" must be encoded.
     */
    /* Eat "//" */
    for ( i = 0; i < 2; i++ ) {
        if ( '/' != *curstr ) {
            nb_parsed_url_free(purl);
            return NULL;
        }
        curstr++;
    }

    /* Check if the user (and password) are specified. */
    userpass_flag = 0;
    tmpstr = curstr;
    while ( '\0' != *tmpstr ) {
        if ( '@' == *tmpstr ) {
            /* Username and password are specified */
            userpass_flag = 1;
            break;
        } else if ( '/' == *tmpstr ) {
            /* End of <host>:<port> specification */
            userpass_flag = 0;
            break;
        }
        tmpstr++;
    }

    /* User and password specification */
    tmpstr = curstr;
    if ( userpass_flag ) {
        /* Read username */
        while ( '\0' != *tmpstr && ':' != *tmpstr && '@' != *tmpstr ) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        purl->username = malloc(sizeof(char) * (len + 1));
        if ( NULL == purl->username ) {
            nb_parsed_url_free(purl);
            return NULL;
        }
        (void)strncpy(purl->username, curstr, len);
        purl->username[len] = '\0';
        /* Proceed current pointer */
        curstr = tmpstr;
        if ( ':' == *curstr ) {
            /* Skip ':' */
            curstr++;
            /* Read password */
            tmpstr = curstr;
            while ( '\0' != *tmpstr && '@' != *tmpstr ) {
                tmpstr++;
            }
            len = tmpstr - curstr;
            purl->password = malloc(sizeof(char) * (len + 1));
            if ( NULL == purl->password ) {
                nb_parsed_url_free(purl);
                return NULL;
            }
            (void)strncpy(purl->password, curstr, len);
            purl->password[len] = '\0';
            curstr = tmpstr;
        }
        /* Skip '@' */
        if ( '@' != *curstr ) {
            nb_parsed_url_free(purl);
            return NULL;
        }
        curstr++;
    }

    if ( '[' == *curstr ) {
        bracket_flag = 1;
    } else {
        bracket_flag = 0;
    }
    /* Proceed on by delimiters with reading host */
    tmpstr = curstr;
    while ( '\0' != *tmpstr ) {
        if ( bracket_flag && ']' == *tmpstr ) {
            /* End of IPv6 address. */
            tmpstr++;
            break;
        } else if ( !bracket_flag && (':' == *tmpstr || '/' == *tmpstr) ) {
            /* Port number is specified. */
            break;
        }
        tmpstr++;
    }
    len = tmpstr - curstr;
    purl->host = malloc(sizeof(char) * (len + 1));
    if ( NULL == purl->host || len <= 0 ) {
        nb_parsed_url_free(purl);
        return NULL;
    }
    (void)strncpy(purl->host, curstr, len);
    purl->host[len] = '\0';
    curstr = tmpstr;

    /* Is port number specified? */
    if ( ':' == *curstr ) {
        curstr++;
        /* Read port number */
        tmpstr = curstr;
        while ( '\0' != *tmpstr && '/' != *tmpstr ) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        purl->port = malloc(sizeof(char) * (len + 1));
        if ( NULL == purl->port ) {
            nb_parsed_url_free(purl);
            return NULL;
        }
        (void)strncpy(purl->port, curstr, len);
        purl->port[len] = '\0';
        curstr = tmpstr;
    }

    /* End of the string */
    if ( '\0' == *curstr ) {
        return purl;
    }

    /* Skip '/' */
    if ( '/' != *curstr ) {
        nb_parsed_url_free(purl);
        return NULL;
    }
    curstr++;

    /* Parse path */
    tmpstr = curstr;
    while ( '\0' != *tmpstr && '#' != *tmpstr  && '?' != *tmpstr ) {
        tmpstr++;
    }
    len = tmpstr - curstr;
    purl->path = malloc(sizeof(char) * (len + 1));
    if ( NULL == purl->path ) {
        nb_parsed_url_free(purl);
        return NULL;
    }
    (void)strncpy(purl->path, curstr, len);
    purl->path[len] = '\0';
    curstr = tmpstr;

    /* Is query specified? */
    if ( '?' == *curstr ) {
        /* Skip '?' */
        curstr++;
        /* Read query */
        tmpstr = curstr;
        while ( '\0' != *tmpstr && '#' != *tmpstr ) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        purl->query = malloc(sizeof(char) * (len + 1));
        if ( NULL == purl->query ) {
            nb_parsed_url_free(purl);
            return NULL;
        }
        (void)strncpy(purl->query, curstr, len);
        purl->query[len] = '\0';
        curstr = tmpstr;
    }

    /* Is fragment specified? */
    if ( '#' == *curstr ) {
        /* Skip '#' */
        curstr++;
        /* Read fragment */
        tmpstr = curstr;
        while ( '\0' != *tmpstr ) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        purl->fragment = malloc(sizeof(char) * (len + 1));
        if ( NULL == purl->fragment ) {
            nb_parsed_url_free(purl);
            return NULL;
        }
        (void)strncpy(purl->fragment, curstr, len);
        purl->fragment[len] = '\0';
        curstr = tmpstr;
    }

    return purl;
}

/*
 * Free memory of parsed url
 */
void
nb_parsed_url_free(nb_parsed_url_t *purl)
{
    if ( NULL != purl ) {
        if ( NULL != purl->scheme ) {
            free(purl->scheme);
        }
        if ( NULL != purl->host ) {
            free(purl->host);
        }
        if ( NULL != purl->port ) {
            free(purl->port);
        }
        if ( NULL != purl->path ) {
            free(purl->path);
        }
        if ( NULL != purl->query ) {
            free(purl->query);
        }
        if ( NULL != purl->fragment ) {
            free(purl->fragment);
        }
        if ( NULL != purl->username ) {
            free(purl->username);
        }
        if ( NULL != purl->password ) {
            free(purl->password);
        }
        free(purl);
    }
}




/*
 * Trim right-side whitespaces
 */
static void
_strrtrim(char *str)
{
    size_t len;
    ssize_t rptr;

    len = strlen(str);
    rptr = len - 1;

    while ( rptr >= 0 ) {
        if ( ' ' == str[rptr] ) {
            str[rptr] = '\0';
        } else {
            break;
        }
        rptr--;
    }
}

/*
 * Eat token by the next space
 */
static char *
_eat_by_space(const char *buf, size_t sz, char **remain)
{
    size_t pos;
    char *str;
    int space;

    /* Search the separator */
    pos = 0;
    space = 0;
    for ( ;; ) {
        if ( ' ' == buf[pos] ) {
            /* Found space */
            space = 1;
            break;
        } else if ( '\r' == buf[pos] || '\n' == buf[pos] || '\0' == buf[pos] ) {
            /* Found newline or other special chars */
            break;
        } else if ( pos + 1 == sz ) {
            /* End-of-buffer */
            break;
        }
        pos++;
    }

    /* Allocate for the token */
    str = malloc(sizeof(char) * pos + 1);
    if ( NULL == str ) {
        return NULL;
    }
    if ( pos > 0 ) {
        (void)memcpy(str, buf, pos);
    }
    str[pos] = '\0';

    /* Set remaining to a pointer */
    if ( NULL != remain ) {
        *remain = (char *)buf + pos + space;
    }

    return str;
}
/*
 * Eat token by the next newline
 */
static char *
_eat_by_newline(const char *buf, size_t sz, char **remain)
{
    size_t pos;
    char *str;
    int space;

    /* Search the separator */
    pos = 0;
    space = 0;
    for ( ;; ) {
        if ( '\n' == buf[pos] ) {
            /* Found LF */
            space = 1;
            break;
        } else if ( '\r' == buf[pos] && pos + 1 < sz && '\n' == buf[pos+1] ) {
            /* Found CR-LF */
            space = 2;
            break;
        } else if ( '\0' == buf[pos] ) {
            /* Found special chars */
            break;
        } else if ( pos + 1 == sz ) {
            /* End-of-buffer */
            break;
        }
        pos++;
    }

    /* Allocate for the token */
    str = malloc(sizeof(char) * (pos + 1));
    if ( NULL == str ) {
        return NULL;
    }
    if ( pos > 0 ) {
        (void)memcpy(str, buf, pos);
    }
    str[pos] = '\0';

    /* Set remaining to a pointer */
    if ( NULL != remain ) {
        *remain = (char *)buf + pos + space;
    }

    return str;
}

/*
 * Is control character in HTTP
 */
static __inline__ int
_isctl(int c)
{
    if ( c <= 31 || c >= 127 ) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * Delete an attribute list
 */
static void
_attr_list_delete(nb_http_header_attr_list_t *attrs)
{
    nb_http_header_attr_list_t *tmp;

    while ( NULL != attrs ) {
        tmp = attrs->next;
        free(attrs->attr->key);
        free(attrs->attr->value);
        free(attrs->attr);
        free(attrs);
        attrs = tmp;
    }
}

/*
 * Parse request header
 */
static nb_http_header_attr_list_t *
_parse_attrs(const char *buf, size_t sz)
{
    nb_http_header_attr_list_t *attrs;
    nb_http_header_attr_list_t *nattr;
    size_t pos;
    size_t bpos;
    size_t prevsz;
    int space;
    char *value;
    char *tmpstr;
    int errno_save;

    attrs = NULL;

    pos = 0;
    while ( pos < sz ) {
        /* Check end-of-header */
        if ( '\n' == buf[pos] ) {
            pos++;
            break;
        } else if ( '\r' == buf[pos] && pos + 1 < sz && '\n' == buf[pos+1] ) {
            pos += 2;
            break;
        }

        /* Allocate entry */
        nattr = malloc(sizeof(nb_http_header_attr_list_t));
        if ( NULL == nattr ) {
            errno_save = errno;
            if ( NULL != attrs ) {
                _attr_list_delete(attrs->head);
            }
            errno = errno_save;
            return NULL;
        }
        nattr->attr = malloc(sizeof(nb_http_header_attr_t));
        if ( NULL == nattr->attr ) {
            errno_save = errno;
            if ( NULL != attrs ) {
                _attr_list_delete(attrs->head);
            }
            free(nattr);
            errno = errno_save;
            return NULL;
        }
        nattr->head = NULL;
        nattr->prev = NULL;
        nattr->next = NULL;

        /* Key */
        bpos = pos;
        space = 0;
        while ( pos < sz ) {
            if ( ':' == buf[pos] ) {
                space = 1;
                break;
            } else if ( _isctl(buf[pos]) ) {
                /* Invalid format */
                if ( NULL != attrs ) {
                    _attr_list_delete(attrs->head);
                }
                free(nattr->attr);
                free(nattr);
                errno = EINVAL;
                return NULL;
            }
            pos++;
        }
        nattr->attr->key = malloc(sizeof(char) * (pos - bpos + 1));
        if ( NULL == nattr->attr->key ) {
            errno_save = errno;
            if ( NULL != attrs ) {
                _attr_list_delete(attrs->head);
            }
            free(nattr->attr);
            free(nattr);
            errno = errno_save;
            return NULL;
        }
        (void)memcpy(nattr->attr->key, buf + bpos, pos - bpos);
        nattr->attr->key[pos - bpos] = '\0';
        /* Trim right-side whitespaces */
        _strrtrim(nattr->attr->key);
        pos += space;

        /* Skip white space */
        while ( pos < sz ) {
            if ( ' ' != buf[pos] ) {
                break;
            }
            pos++;
        }

        /* Value */
        value = NULL;
        prevsz = 0;
        while ( pos < sz ) {
            /* Parse value */
            bpos = pos;
            space = 0;
            while ( pos < sz ) {
                if ( '\n' == buf[pos] ) {
                    space = 1;
                    break;
                } else if ( '\r' == buf[pos] && pos + 1 < sz
                            && '\n' == buf[pos+1] ) {
                    space = 2;
                    break;
                } else if ( '\0' == buf[pos] ) {
                    /* Should be ascii */
                    if ( NULL != value ) {
                        free(value);
                    }
                    if ( NULL != attrs ) {
                        _attr_list_delete(attrs->head);
                    }
                    free(nattr->attr->key);
                    free(nattr->attr);
                    free(nattr);
                    errno = EINVAL;
                    return NULL;
                }
                pos++;
            }
            /* Resize the allocated memory for this value  */
            tmpstr = realloc(value, sizeof(char) * (prevsz + pos - bpos + 1));
            if ( NULL == tmpstr ) {
                errno_save = errno;
                if ( NULL != value ) {
                    free(value);
                }
                if ( NULL != attrs ) {
                    _attr_list_delete(attrs->head);
                }
                free(nattr->attr->key);
                free(nattr->attr);
                free(nattr);
                errno = errno_save;
                return NULL;
            }
            value = tmpstr;
            (void)memcpy(value + prevsz, buf + bpos, pos - bpos);
            value[prevsz + pos - bpos] = '\0';
            prevsz += pos - bpos;
            pos += space;

            /* Continue? */
            if ( ' ' == buf[pos] ) {
                pos++;
            } else if ( '\t' == buf[pos] ) {
                /* Nothing */
            } else {
                break;
            }
        }
        nattr->attr->value = value;

        /* Append */
        if ( NULL != attrs ) {
            /* Inherit the pointer to head */
            nattr->head = attrs->head;
            /* Set this as next */
            attrs->next = nattr;
        } else {
            /* Set this as head */
            nattr->head = nattr;
        }
        /* Set the previous pointer */
        nattr->prev = attrs;
        /* Update the tail */
        attrs = nattr;
    }

    if ( NULL != attrs ) {
        return attrs->head;
    } else {
        return NULL;
    }
}

/*
 * Parse HTTP header
 */
nb_http_header_t *
nb_parse_http_header(const char *buf, size_t sz)
{
    nb_http_header_t *hdr;
    char *cptr;
    size_t rsz;
    char *remain;
    int errno_saved;

    /* Allocate the structure */
    hdr = malloc(sizeof(nb_http_header_t));
    if ( NULL == hdr ) {
        return NULL;
    }
    bzero(hdr, sizeof(nb_http_header_t));

    cptr = (char *)buf;
    rsz = sz;

    /* Method */
    hdr->method = _eat_by_space(cptr, rsz, &remain);
    rsz = rsz - (size_t)(remain - cptr);
    cptr = remain;
    if ( NULL == hdr->method ) {
        errno_saved = errno;
        free(hdr);
        errno = errno_saved;
        return NULL;
    }

    /* URI */
    hdr->uri = _eat_by_space(cptr, rsz, &remain);
    rsz = rsz - (size_t)(remain - cptr);
    cptr = remain;
    if ( NULL == hdr->uri ) {
        errno_saved = errno;
        free(hdr->method);
        free(hdr);
        errno = errno_saved;
        return NULL;
    }

    /* Version */
    hdr->version = _eat_by_newline(cptr, rsz, &remain);
    rsz = rsz - (size_t)(remain - cptr);
    cptr = remain;
    if ( NULL == hdr->version ) {
        errno_saved = errno;
        free(hdr->uri);
        free(hdr->method);
        free(hdr);
        errno = errno_saved;
        return NULL;
    }

    /* Attributes */
    errno = 0;
    hdr->attrs = _parse_attrs(cptr, rsz);;
    if ( NULL == hdr->attrs && errno ) {
        errno_saved = errno;
        free(hdr->version);
        free(hdr->uri);
        free(hdr->method);
        free(hdr);
        errno = errno_saved;
        return NULL;
    }

    return hdr;
}

/*
 * Delete HTTP header
 */
void
nb_http_header_delete(nb_http_header_t *hdr)
{
    if ( NULL != hdr->method ) {
        free(hdr->method);
    }
    if ( NULL != hdr->uri ) {
        free(hdr->uri);
    }
    if ( NULL != hdr->version ) {
        free(hdr->version);
    }
    if ( NULL != hdr->attrs ) {
        _attr_list_delete(hdr->attrs);
    }
    free(hdr);
}

/*
 * Get "Content-Length" attribute
 */
off_t
nb_http_header_get_content_length(nb_http_header_t *hdr)
{
    off_t clen;
    char *remain;
    nb_http_header_attr_list_t *attrs;

    errno = 0;
    attrs = hdr->attrs;
    clen = -1;
    while ( NULL != attrs ) {
        if ( 0 == strcasecmp(attrs->attr->key, "Content-Length") ) {
            clen = strtoll(attrs->attr->value, &remain, 10);
            if ( 0 != strcmp("", remain) || clen < 0 ) {
                /* Invalid */
                clen = -1;
            }
            break;
        }
        attrs = attrs->next;
    }

    return clen;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
