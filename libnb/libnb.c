/*_
 * Copyright 2013-2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include "netbench_private.h"
#include "netbench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

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
 * Get data via HTTP
 */
int
nb_http_get(const char *url, char **resstr, off_t *reslen)
{
    nb_parsed_url_t *purl;
    int sock;
    struct addrinfo hints, *res, *ressave;
    char *port;
    char *path;
    int err;
    char servhost[NI_MAXHOST];
    char servservice[NI_MAXSERV];

    nb_http_header_t *reqhdr;
    char *reqhdrstr;
    off_t reqhdrlen;
    char *reqbdystr;
    off_t reqbdylen;

    char req[4096];
    char buf[4096];
    ssize_t nw;
    ssize_t nr;
    size_t tsize;
    off_t clen;
    struct timeval timeout;
    double gtimeout = 60.0;


    /* Parse the URL */
    purl = nb_parse_url(url);
    if ( NULL == purl ) {
        return -1;
    }
    /* Only the scheme "http" is supported. */
    if ( strcasecmp("http", purl->scheme) ) {
        nb_parsed_url_free(purl);
        return -1;
    }

    port = purl->port;
    if ( NULL == port ) {
        /* Set default port */
        port = "80";
    }

    /* Open a socket */
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    err = getaddrinfo(purl->host, port, &hints, &res);
    if ( 0 != err ) {
        /* Error */
        nb_parsed_url_free(purl);
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
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Resolve the host and service */
    err = getnameinfo(res->ai_addr, res->ai_addrlen, servhost, sizeof(servhost),
                      servservice, sizeof(servservice),
                      NI_NUMERICHOST | NI_NUMERICSERV);
    if ( 0 != err ) {
        /* Error */
    }

    /* Free */
    freeaddrinfo(ressave);

    /* Set timeout */
    timeout.tv_sec = (time_t)gtimeout;
    timeout.tv_usec = (suseconds_t)((gtimeout - (time_t)gtimeout) * 1000000);
    if ( 0 != setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                         sizeof(struct timeval)) ) {
        /* Error */
        (void)close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Build and send a request */
    path = _build_request_uri(purl);
    if ( NULL ==  path ) {
        /* Error */
        (void)close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }
    snprintf(req, sizeof(req), "GET %.1024s HTTP/1.1\r\n"
             "Host: %.1024s\r\n"
             "User-Agent: %s\r\n"
             "Connection: close\r\n\r\n", path, purl->host, USER_AGENT);
    free(path);

    /* Send the header */
    nw = send(sock, req, strlen(req), 0);
    if ( nw != strlen(req) ) {
        close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Read response header */
    err = _read_response_header(sock, &reqhdrstr, &reqhdrlen, &reqbdystr,
                                &reqbdylen);
    if ( err < 0 ) {
        /* Error */
        close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Parse the response header */
    reqhdr = nb_parse_http_header(reqhdrstr, (size_t)reqhdrlen);
    if ( NULL == reqhdr ) {
        /* Error */
        free(reqhdrstr);
        if ( NULL != reqbdystr ) {
            free(reqbdystr);
        }
        close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Get content length */
    clen = nb_http_header_get_content_length(reqhdr);

    /* Allocate */
    *reslen = clen;
    if ( clen > 0 ) {
        *resstr = malloc(sizeof(char) * (*reslen));
        if ( NULL == *resstr ) {
            nb_http_header_delete(reqhdr);
            free(reqhdrstr);
            if ( NULL != reqbdystr ) {
                free(reqbdystr);
            }
            close(sock);
            nb_parsed_url_free(purl);
            return -1;
        }
    } else {
        *resstr = NULL;
    }
    if ( reqbdylen > 0 && reqbdylen <= clen ) {
        (void)memcpy(*resstr, reqbdystr, reqbdylen);
        tsize = (size_t)reqbdylen;
    }

    /* Download the body */
    while ( (nr = recv(sock, buf, sizeof(buf), 0)) > 0 ) {
        (void)memcpy(*resstr + tsize, buf, nr);
        tsize += nr;
    }

    nb_http_header_delete(reqhdr);
    free(reqhdrstr);
    if ( NULL != reqbdystr ) {
        free(reqbdystr);
    }
    shutdown(sock, SHUT_RDWR);
    close(sock);
    nb_parsed_url_free(purl);

    if ( tsize < clen ) {
        if ( NULL != resstr ) {
            free(resstr);
        }
        return -1;
    }

    return 0;
}

/*
 * Post data via HTTP
 */
int
nb_http_post(const char *url, const char *content_type, const char *data,
             size_t sz, char **resstr, off_t *reslen)
{
    nb_parsed_url_t *purl;
    int sock;
    struct addrinfo hints, *res, *ressave;
    char *port;
    char *path;
    int err;
    char servhost[NI_MAXHOST];
    char servservice[NI_MAXSERV];

    nb_http_header_t *reqhdr;
    char *reqhdrstr;
    off_t reqhdrlen;
    char *reqbdystr;
    off_t reqbdylen;

    char req[4096];
    char buf[4096];
    ssize_t nw;
    ssize_t nr;
    size_t tsize;
    off_t clen;
    struct timeval timeout;
    double gtimeout = 60.0;

    /* Parse the URL */
    purl = nb_parse_url(url);
    if ( NULL == purl ) {
        return -1;
    }
    /* Only the scheme "http" is supported. */
    if ( strcasecmp("http", purl->scheme) ) {
        nb_parsed_url_free(purl);
        return -1;
    }

    port = purl->port;
    if ( NULL == port ) {
        /* Set default port */
        port = "80";
    }

    /* Open a socket */
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    err = getaddrinfo(purl->host, port, &hints, &res);
    if ( 0 != err ) {
        /* Error */
        nb_parsed_url_free(purl);
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
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Resolve the host and service */
    err = getnameinfo(res->ai_addr, res->ai_addrlen, servhost, sizeof(servhost),
                      servservice, sizeof(servservice),
                      NI_NUMERICHOST | NI_NUMERICSERV);
    if ( 0 != err ) {
        /* Error */
    }

    /* Free */
    freeaddrinfo(ressave);

    /* Set timeout */
    timeout.tv_sec = (time_t)gtimeout;
    timeout.tv_usec = (suseconds_t)((gtimeout - (time_t)gtimeout) * 1000000);
    if ( 0 != setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                         sizeof(struct timeval)) ) {
        /* Error */
        (void)close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Build and send a request */
    path = _build_request_uri(purl);
    if ( NULL ==  path ) {
        /* Error */
        (void)close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }
    snprintf(req, sizeof(req), "POST %.1024s HTTP/1.1\r\n"
             "Host: %.1024s\r\n"
             "User-Agent: %s\r\n"
             "Content-Type: %.1024s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n", path, purl->host, USER_AGENT,
             content_type, sz);
    free(path);

    /* Send the header */
    nw = send(sock, req, strlen(req), 0);
    if ( nw != strlen(req) ) {
        close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Upload the body */
    ssize_t sent = 0;
    while ( (nw = send(sock, data+sent, sz - sent, 0)) > 0 ) {
        sent += nw;
    }
    if ( nw < 0 ) {
        /* Error */
        close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Read response header */
    err = _read_response_header(sock, &reqhdrstr, &reqhdrlen, &reqbdystr,
                                &reqbdylen);
    if ( err < 0 ) {
        /* Error */
        close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Parse the response header */
    reqhdr = nb_parse_http_header(reqhdrstr, (size_t)reqhdrlen);
    if ( NULL == reqhdr ) {
        /* Error */
        free(reqhdrstr);
        if ( NULL != reqbdystr ) {
            free(reqbdystr);
        }
        close(sock);
        nb_parsed_url_free(purl);
        return -1;
    }

    /* Get content length */
    clen = nb_http_header_get_content_length(reqhdr);

    /* Allocate */
    *reslen = clen;
    if ( clen > 0 ) {
        *resstr = malloc(sizeof(char) * (*reslen));
        if ( NULL == *resstr ) {
            nb_http_header_delete(reqhdr);
            free(reqhdrstr);
            if ( NULL != reqbdystr ) {
                free(reqbdystr);
            }
            close(sock);
            nb_parsed_url_free(purl);
            return -1;
        }
    } else {
        *resstr = NULL;
    }
    if ( reqbdylen > 0 && reqbdylen <= clen ) {
        (void)memcpy(*resstr, reqbdystr, reqbdylen);
        tsize = (size_t)reqbdylen;
    }

    /* Download the body */
    while ( (nr = recv(sock, buf, sizeof(buf), 0)) > 0 ) {
        (void)memcpy(*resstr + tsize, buf, nr);
        tsize += nr;
    }

    nb_http_header_delete(reqhdr);
    free(reqhdrstr);
    if ( NULL != reqbdystr ) {
        free(reqbdystr);
    }
    shutdown(sock, SHUT_RDWR);
    close(sock);
    nb_parsed_url_free(purl);

    if ( tsize < clen ) {
        if ( NULL != resstr ) {
            free(resstr);
        }
        return -1;
    }

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
