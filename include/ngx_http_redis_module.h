#ifndef _NGX_HTTP_REDIS_MODULE_H_
#define _NGX_HTTP_REDIS_MODULE_H_
#include <ngx_http.h>
#include <ngx_buddy.h>
#include <ngx_redis_cache.h>



/* 宏常量 */
/* 默认的本地redis缓存大小 1M */
#define _DEFAULT_REDIS_BUFF_SIZE            (1024 * 1024) 
/* 默认的redis host 127.0.0.1 */
#define _DEFAULT_REDIS_HOST                 "127.0.0.1"
/* 默认的redis端口 6379 */
#define _DEFAULT_REDIS_PORT                 6379
/* 默认 redis 连接超时时间 10s */
#define _DEFAULT_REDIS_CONN_TIMEOUT         10000
/* 默认 redis 请求发送超时时间 10s */
#define _DEFAULT_REDIS_SEND_TIMEOUT         10000
/* 默认 redis 读取发送超时时间 10s */
#define _DEFAULT_REDIS_READ_TIMEOUT         10000
/* 默认的redis 响应的类型 */
#define _DEFAULT_REDIS_CONTENT_TYPE         "text/plain"
/* 伙伴系统魔数 */
#define _REDIS_BUDDY_SYS_MAGIC              19930820U
/* redis 本地缓存默认超时时间, 单位毫秒 */
#define _DEFAULT_REDIS_CACHE_TIMEOUT        60000
/* redis 本地缓存默认hash桶大小 */
#define _DEFAULT_REDIS_CACHE_BUCKET_SIZE    1031

/* 允许解析的行的最长长度 */
#define _MAXLINE                    8192
#define _HASH_MAX_SIZE              100 
#define _HASH_BUCKET_SIZE           1024
#define _HASH_NAME                  "redis_headers_hash"

/* 请求redis的状态 */
#define _STATUS_STARTING            0
#define _STATUS_READING_AUTH        1
#define _STATUS_SELECT_CH           2
#define _STATUS_READING_LEN         4

/* redis 关键字类型 */
/* 字符串类型 */
#define _REDIS_KEY_TYPE_STR         0
/* nginx 变量类型 */
#define _REDIS_KEY_TYPE_ARG         1


/* 数据结构定义 */
/* 存储redis关键字配置的联合 */
union redis_key_un {
    /* 如果读取的key是nginx变量,这个保存变量的索引 */
    ngx_int_t   rediskey_idx;
    /* 如果不是变量,直接从配置文件中读取 */
    ngx_str_t   rediskey_conf;
};

struct rediskey_conf_s {
    ngx_int_t           rediskey_type;
    union redis_key_un  rediskey_un;
};
typedef struct ngx_http_redis_module_loc_conf_s {
    ngx_flag_t                  visit;              /* 是否访问redis, 访问redis的开关 */
    ngx_str_t                   host;               /* redis host */
    ngx_int_t                   port;               /* redis port */
    ngx_str_t                   username;           /* redis 用户名 */
    ngx_str_t                   password;           /* redis 密码 */
    ngx_flag_t                  cache;              /* 是否开启本地缓存 */
    ngx_flag_t                  cachemiss;          /* 是否缓存未命中 */
    ngx_msec_t                  cacheto;            /* 缓存过期时间 */
    ngx_uint_t                  hash_bucket_size;   /* redis local cache hash bucket size */
    size_t                      bsize;              /* redis 本地缓存大小 */
    ngx_str_t                   backend;            /* 实际redis服务器的ip地址,点分十进制 */
    ngx_http_upstream_conf_t    upconf;             /* upstream 配置信息 */
    struct rediskey_conf_s      rediskey;           /* redis 的 key */
    ngx_str_t                   redis_ch;           /* redis 通道 */
    /* ngx_str_t                   redisval;   /1* 存储val的地方 *1/ */
    struct sockaddr_in          conn;
/* #if (NGX_HAVE_INET6)                    /1* ipv6 support *1/ */
/*     struct sockaddr_in6      connv6; */
/* #endif */
    ngx_str_t                   redis_content_type;
    ngx_buddy_t                 *nb;
    ngx_redis_cache_t           *nr;
} ngx_http_redis_module_loc_conf_t;


/* 请求上线文信息 */
typedef struct redis_module_req_ctx_s {
    ngx_http_upstream_t                 *upstream;
    ngx_http_request_t                  *request;
    ngx_str_t                           rediskey;   /* redis 最终要被获取的key */
    ngx_int_t                           status;
    ngx_str_t                           header;     /* 保存认证信息及值的长度, header.data = buf */
    ngx_int_t                           hidx;
    ngx_int_t                           lenidx;
    ngx_int_t                           vallen;
    u_char                              buf[_MAXLINE];  /* 解析redis响应使用的buff */
    ngx_http_redis_module_loc_conf_t    *rlcf;
    ngx_redis_cache_node_t              *rcn;
} redis_module_req_ctx_t;



/* 函数原型 */
static void *http_redis_create_loc_conf(ngx_conf_t *cf);
static char *http_redis_conf_set_key(ngx_conf_t *cf, ngx_command_t *cmd, 
        void *conf);
static char *http_redis_merge_loc_conf(ngx_conf_t *cf, void *prev, void *conf);
static ngx_int_t http_redis_postconfig(ngx_conf_t *cf);



/* 和 upstream 相关的回调函数 */
static ngx_int_t http_redis_upstream_create_request(ngx_http_request_t *r);
static ngx_int_t http_redis_upstream_process_header(ngx_http_request_t *r);
static ngx_int_t http_redis_upstream_filter_init(void *data);
static ngx_int_t http_redis_upstream_filter(void *data, ssize_t bytes);
static void http_redis_upstream_finalize_request(ngx_http_request_t *r, ngx_int_t rc);
static ngx_int_t http_redis_module_handler(ngx_http_request_t *r);



/* 宏函数 */
/* #define _ckval_set(param, val, setval)                                      \ */
/*     do {                                                                    \ */
/*         typeof(param) _param = (param), _val = (val), _setval = (setval);   \ */
/*         if(_param == _val) {                                                \ */
/*             (param) = _setval;                                              \ */
/*         }                                                                   \ */
/*     } while(0) */

#define _hash_str(str) ({                                               \
    char *_c = (str);                                                   \
    ngx_uint_t  _hash = 0;                                              \
    while(*_c != 0) {                                                   \
        _hash = ngx_hash(_hash, *_c);                                   \
        _c++;                                                           \
    }                                                                   \
    _hash;                                                              \
})

#define _readline(rbuf, wbuf, wpos) do {                                    \
            ngx_buf_t   *_b = (rbuf);                                       \
            ngx_str_t   *_w = (wbuf);                                       \
            u_char      *_p, _c;                                            \
            ngx_int_t   *_l = (wpos), _flag = 0;                            \
            for(_p = _b->pos; _p < _b->last; _p++, _b->pos++) {             \
                if( *_l >= _MAXLINE - 1 ) { /* 超出缓存区范围 */            \
                        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,   \
                                "header is out of buffer size");            \
                    return NGX_ERROR;                                       \
                }                                                           \
                _c = *_p;                                                   \
                _w->data[*_l] = _c;                                         \
                *_l += 1;                                                   \
                _w->len = *_l;                                              \
                if( _c == '\n' && *_l > 1 && _w->data[*_l-2] == '\r' ) {    \
                    _flag = 1; /* 一行读取完毕 */                           \
                    break;                                                  \
                }                                                           \
            }                                                               \
            _b->pos++;                                                      \
            if(_flag == 0) {                                                \
                return NGX_AGAIN;                                           \
            }                                                               \
} while(0)

#define _set_httpstatus_contenlen(u, l, s) do {                     \
    typeof(u) _u = (u);                                             \
    typeof(l) _l = (l);                                             \
    typeof(s) _s = (s);                                             \
    if(_u->state) {                                                 \
        _u->state->status = _s;                                     \
    }                                                               \
    _u->headers_in.status_n = _s;                                   \
    _u->headers_in.content_length_n = _l; /* \r\n */                \
    _u->length = _l;                                                \
} while(0)


#endif
