#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <string.h>

/*
  Usage:
    send_udp/post_http(c,
            INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v2"), INFLUX_F_STR("s", "string"), INFLUX_F_FLT("f", 28.39, 2),
        INFLUX_NEW_LINE,
            INFLUX_MEAS("bar"), INFLUX_F_INT("i", 1048576), INFLUX_F_BOL("b", 1), INFLUX_TS(1512722735522840439),
        INFLUX_END);

  **NOTICE**: For best performance you should sort tags by key before sending them to the database.
              The sort should match the results from the [Go bytes.Compare function](https://golang.org/pkg/bytes/#Compare).
 */

#define INFLUX_NEW_LINE       IF_TYPE_NEW_LINE
#define INFLUX_MEAS(m)        IF_TYPE_MEAS, m
#define INFLUX_TAG(k, v)      IF_TYPE_TAG, k, v
#define INFLUX_F_STR(k, v)    IF_TYPE_FIELD_STRING, k, v
#define INFLUX_F_FLT(k, v, p) IF_TYPE_FIELD_FLOAT, k, (double)v, (int)p
#define INFLUX_F_INT(k, v)    IF_TYPE_FIELD_INTEGER, k, (unsigned long long)v
#define INFLUX_F_BOL(k, v)    IF_TYPE_FIELD_BOOLEAN, k, (v ? 1 : 0)
#define INFLUX_TS(ts)         IF_TYPE_TIMESTAMP, (unsigned long long)ts
#define INFLUX_END            IF_TYPE_ARG_END

typedef struct _influx_client_t
{
    char* host;
    char* db;
    char* usr;
    char* pwd;
    int   port;
} influx_client_t;

int post_http(influx_client_t* c, ...);
int send_udp(influx_client_t* c, ...);

#define IF_TYPE_ARG_END       0
#define IF_TYPE_NEW_LINE      1
#define IF_TYPE_MEAS          2
#define IF_TYPE_TAG           3
#define IF_TYPE_FIELD_STRING  4
#define IF_TYPE_FIELD_FLOAT   5
#define IF_TYPE_FIELD_INTEGER 6
#define IF_TYPE_FIELD_BOOLEAN 7
#define IF_TYPE_TIMESTAMP     8

int _escaped_append(char** dest, size_t* len, size_t* used, const char* src, const char* escape_seq);
int _format_line(char** buf, va_list ap);
char _get_next_char(int sock, struct iovec* iv, int* pos)
{
    if(*pos < (int)iv->iov_len)
        return *((char*)iv->iov_base + (*pos)++);
    if((*pos = recv(sock, iv->iov_base, iv->iov_len, 0)) < 0)
        return 0;
    iv->iov_len = *pos;
    return *((char*)iv->iov_base + (*pos = 0));
}

int post_http(influx_client_t* c, ...)
{
    va_list ap;
    struct iovec iv[2];
    struct sockaddr_in addr;
    int sock, ret_code = 0, content_length = 0, len = 0;
    char ch;
    
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(c->port);
    addr.sin_addr.s_addr = inet_addr(c->host);
    if(addr.sin_addr.s_addr == INADDR_NONE)
        return -2;

    if(connect(sock, (struct sockaddr*)(&addr), sizeof(addr)) < 0)
        return -3;

    va_start(ap, c);
    len = _format_line((char**)&iv[1].iov_base, ap);
    va_end(ap);
    if(len < 0)
        return -4;
    iv[1].iov_len = len;

    if(!(iv[0].iov_base = (char*)malloc(iv[0].iov_len = 0x100))) {
        free(iv[1].iov_base);
        return -5;
    }
    for(;;) {
        len = snprintf((char*)iv[0].iov_base, iv[0].iov_len, "POST /write?db=%s&u=%s&p=%s HTTP/1.1\r\nHost: %s\r\nContent-Length: %zd\r\n\r\n", c->db, c->usr ? c->usr : "", c->pwd ? c->pwd : "", c->host, iv[1].iov_len);
        if(len > (int)iv[0].iov_len && !(iv[0].iov_base = (char*)realloc(iv[0].iov_base, iv[0].iov_len *= 2))) {
            free(iv[1].iov_base);
            return -6;
        }
        else
            break;
    }
    iv[0].iov_len = len;

    if(writev(sock, iv, 2) < (int)(iv[0].iov_len + iv[1].iov_len)) {
        ret_code = -7;
        goto END;
    }

#define _GET_NEXT_CHAR() ch = _get_next_char(sock, &iv[0], &len)
#define _LOOP_NEXT(statement) for(;;) { if(!(_GET_NEXT_CHAR())) { ret_code = -8; goto END; } statement }
#define _UNTIL(c) _LOOP_NEXT( if(ch == c) break; )
#define _GET_NUMBER(n) _LOOP_NEXT( if(ch >= '0' && ch <= '9') n = n * 10 + (ch - '0'); else break; )
#define _(c) if((_GET_NEXT_CHAR()) != c) break;

    _UNTIL(' ')_GET_NUMBER(ret_code)
    for(;;) {
        _UNTIL('\n')
        switch(_GET_NEXT_CHAR()) {
            case 'C':_('o')_('n')_('t')_('e')_('n')_('t')_('-')
                _('L')_('e')_('n')_('g')_('t')_('h')_(':')_(' ')
                _GET_NUMBER(content_length)
                break;
            case '\r':_('\n')
                // printf("%.*s", (int)(iv[0].iov_len - len), (char*)iv[0].iov_base + len);
                content_length -= iv[0].iov_len - len;
                while(content_length > 0) {
                    if((len = recv(sock, iv[0].iov_base, 
                        content_length < (int)iv[0].iov_len ? content_length : iv[0].iov_len, 0)) < 0) {
                        ret_code = -9;
                        goto END;
                    }
                    // printf("%.*s", len, (char*)iv[0].iov_base);
                    content_length -= len;
                }
                goto END;
        }
        if(!ch) {
            ret_code = -10;
            goto END;
        }
    }
    ret_code = -11;
END:
    free(iv[0].iov_base);
    free(iv[1].iov_base);
    return ret_code / 100 == 2 ? 0 : ret_code;
}
#undef _GET_NEXT_CHAR
#undef _LOOP_NEXT
#undef _UNTIL
#undef _GET_NUMBER
#undef _

int send_udp(influx_client_t* c, ...)
{
    va_list ap;
    char* line = NULL;
    int sock, len = 0;
    struct sockaddr_in addr;
    
    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(c->port);
    addr.sin_addr.s_addr = inet_addr(c->host);
    if(addr.sin_addr.s_addr == INADDR_NONE)
        return -2;

    va_start(ap, c);
    len = _format_line(&line, ap);
    va_end(ap);
    if(len < 0)
        return -3;

    if(sendto(sock, line, len, 0, (struct sockaddr *)&addr, sizeof(addr)) < len) {
        free(line);
        return -4;
    }
    free(line);
    return 0;
}

int _format_line(char** buf, va_list ap)
{
#define _APPEND(fmter...) \
    for(;;) {\
        if((written = snprintf(*buf + used, len - used, ##fmter)) < 0)\
            goto FAIL;\
        if(used + written > len && !(*buf = (char*)realloc(*buf, len *= 2)))\
            return -1;\
        else {\
            used += written;\
            break;\
        }\
    }

    size_t len = 0x100, used = 0;
    int written = 0, type = 0, last_type = IF_TYPE_NEW_LINE;
    unsigned long long i = 0;
    double d = 0.0;

    if(!(*buf = (char*)malloc(len)))
        return -1;

    type = va_arg(ap, int);
    while(type != IF_TYPE_ARG_END) {
        if(type >= IF_TYPE_TAG && type <= IF_TYPE_FIELD_BOOLEAN) {
            if(last_type < IF_TYPE_MEAS || last_type > (type == IF_TYPE_TAG ? IF_TYPE_TAG : IF_TYPE_FIELD_BOOLEAN))
                goto FAIL;
            _APPEND("%c", (last_type <= IF_TYPE_TAG && type > IF_TYPE_TAG) ? ' ' : ',');
            if(_escaped_append(buf, &len, &used, va_arg(ap, char*), ",= "))
                return -2;
            _APPEND("=");
        }
        switch(type) {
            case IF_TYPE_NEW_LINE:
                if(last_type <= IF_TYPE_TAG || last_type > IF_TYPE_TIMESTAMP)
                    goto FAIL;
                _APPEND("\n");
                break;
            case IF_TYPE_MEAS:
                if(last_type != IF_TYPE_NEW_LINE)
                    goto FAIL;
                if(_escaped_append(buf, &len, &used, va_arg(ap, char*), ", "))
                    return -3;
                break;
            case IF_TYPE_TAG:
                if(!(last_type & 0x02))
                    goto FAIL;
                if(_escaped_append(buf, &len, &used, va_arg(ap, char*), ",= "))
                    return -4;
                break;
            case IF_TYPE_FIELD_STRING:
                _APPEND("\"");
                if(_escaped_append(buf, &len, &used, va_arg(ap, char*), "\""))
                    return -5;
                _APPEND("\"");
                break;
            case IF_TYPE_FIELD_FLOAT:
                d = va_arg(ap, double);
                i = va_arg(ap, int);
                _APPEND("%.*lf", (int)i, d);
                break;
            case IF_TYPE_FIELD_INTEGER:
                i = va_arg(ap, unsigned long long);
                _APPEND("%lldi", i);
                break;
            case IF_TYPE_FIELD_BOOLEAN:
                i = va_arg(ap, int);
                _APPEND("%c", i ? 't' : 'f');
                break;
            case IF_TYPE_TIMESTAMP:
                if(!(last_type & 0x04))
                    goto FAIL;
                i = va_arg(ap, unsigned long long);
                _APPEND(" %lld", i);
                break;
            default:
                goto FAIL;
        }
        last_type = type;
        type = va_arg(ap, int);
    }
    if(last_type <= IF_TYPE_TAG)
        goto FAIL;
    return used;
FAIL:
    free(*buf);
    *buf = NULL;
    return -1;
}
#undef _APPEND

int _escaped_append(char** dest, size_t* len, size_t* used, const char* src, const char* escape_seq)
{
    size_t i = 0;

    for(;;) {
        if((i = strcspn(src, escape_seq)) > 0) {
            if(*used + i > *len && !(*dest = (char*)realloc(*dest, (*len) *= 2)))
                return -1;
            strncpy(*dest + *used, src, i);
            *used += i;
            src += i;
        }
        if(*src) {
            if(*used + 2 > *len && !(*dest = (char*)realloc(*dest, (*len) *= 2)))
                return -2;
            (*dest)[(*used)++] = '\\';
            (*dest)[(*used)++] = *src++;
        }
        else
            return 0;
    }
    return 0;
}