#include <ngx_http_redis_module.h>
#include <ngx_redis_common.h>

extern volatile ngx_msec_t      ngx_current_msec;
extern volatile ngx_time_t      *ngx_cached_time;

#define _msec_now       ((ngx_msec_t )(ngx_cached_time->msec + 1000 * ngx_cached_time->sec))

/* 静态变量 */
/* redis认证信息 */
static ngx_str_t redisAuth = ngx_string("AUTH %V %V\r\n");
/* redis 选择库通道的命令 */
static ngx_str_t redisSelect = ngx_string("SELECT %V\r\n");
/* redis 获取key值的命令 */
static ngx_str_t redisGet = ngx_string("GET %V\r\n");

static ngx_str_t  redis_proxy_hide_headers[] = {
    ngx_string("Date"),
    ngx_string("X-Pad"),
    ngx_string("X-Accel-Expires"),
    ngx_string("X-Accel-Redirect"),
    ngx_string("X-Accel-Limit-Rate"),
    ngx_string("X-Accel-Buffering"),
    ngx_string("X-Accel-Charset"),
    ngx_null_string
};

static ngx_command_t http_redis_commands[] = {
    {
        ngx_string("redis-visit"),                  /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_flag_slot,                     /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 visit),                            /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-host"),                   /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_str_slot,                      /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 host),                             /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-port"),                   /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_num_slot,                      /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 port),                             /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-user"),                   /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_str_slot,                      /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 username),                         /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-password"),               /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_str_slot,                      /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 password),                         /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-cache"),                  /* name, 配置参数名 */
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_flag_slot,                     /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 cache),                            /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-cache-miss"),             /* name, 配置参数名 */
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_flag_slot,                     /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 cachemiss),                        /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-cache-timeout"),          /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_msec_slot,                     /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 cacheto),                          /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-cache-hash-size"),        /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_num_slot,                      /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 hash_bucket_size),                 /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-cache-size"),             /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_size_slot,                     /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 bsize),                            /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-conn-timeout"),           /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_msec_slot,                     /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                    upconf.connect_timeout),        /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-send-timeout"),           /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_msec_slot,                     /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 upconf.send_timeout),              /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-read-timeout"),           /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_msec_slot,                     /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 upconf.read_timeout),              /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-select"),                 /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_str_slot,                      /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 redis_ch),                         /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-get"),                    /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        http_redis_conf_set_key,                    /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        0,                                          /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    {
        ngx_string("redis-content-type"),           /* name, 配置参数名 */
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF 
        | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,       /* type, 参数类型 */
        ngx_conf_set_str_slot,                      /* set, 参数处理函数 */
        NGX_HTTP_LOC_CONF_OFFSET,                   /* conf, 配置所在位置 */
        offsetof(ngx_http_redis_module_loc_conf_t, 
                 redis_content_type),               /* offset, 和内置set函数配合使用 */
        NULL,                                       /* post, 指向在set函数中需要用的数据 */
    },
    ngx_null_command,
};


static ngx_http_module_t http_redis_module_ctx = {
    NULL,                           /* preconfiguration 读取配置文件之前调用 */
    http_redis_postconfig,          /* postconfiguration 读取配置文件之后调用 */
    NULL,                           /* create_main_conf  创建http{}下存储配置的数据结构 */
    NULL,                           /* init_main_conf 初始化http{}下的存储配置的数据结构 */
    NULL,                           /* create_srv_conf 创建server{}下存储配置的数据结构 */
    NULL,                           /* merge_srv_conf  合并 http{} 和 server{} 冲突的配置 */
    http_redis_create_loc_conf,     /* create_loc_conf 创建location{}下存储配置的数据结构 */
    http_redis_merge_loc_conf,      /* merge_loc_conf 合并 server{}和location{}冲突的配置 */
};

ngx_module_t ngx_http_redis_module = {
    NGX_MODULE_V1,
    &http_redis_module_ctx,     /* ctx */
    http_redis_commands,        /* commands */
    NGX_HTTP_MODULE,            /* type  模块所属的类型 */
    NULL,                       /* init_master 初始化 master 进程时被回调,目前没用 */
    NULL,                       /* init_module 初始化模块时被回调 */
    NULL,                       /* init_process 初始化 worker 进程时被回调 */
    NULL,                       /* init_thread  暂时没用 */
    NULL,                       /* exit_thread 暂时没用 */
    NULL,                       /* exit_process 退出 worker 进程前被回调 */
    NULL,                       /* exit_master 退出 master 进程前被回调 */
    NGX_MODULE_V1_PADDING,
};

/**
 * INFO: 从测试情况来看,函数的调用顺序如下
 *  1. create_main_conf
 *  2. 配置项设置函数
 *  3. init_main_conf
 *  4. postconfiguration
 *  5. init_process
 *  6. exit_process     reload 或者 stop 
 *  7. exit_master      stop
 */


/* 函数定义 */
static void *
http_redis_create_loc_conf(ngx_conf_t *cf) 
{
    ngx_http_redis_module_loc_conf_t *redis_cf;

    redis_cf = _ngx_pcalloc_pool(cf->pool, ngx_http_redis_module_loc_conf_t, NULL);

    redis_cf->visit = NGX_CONF_UNSET;
    redis_cf->port = NGX_CONF_UNSET;
    redis_cf->cache = NGX_CONF_UNSET;
    redis_cf->cachemiss = NGX_CONF_UNSET; 
    redis_cf->cacheto = NGX_CONF_UNSET_MSEC;
    redis_cf->hash_bucket_size = NGX_CONF_UNSET_UINT;
    redis_cf->bsize = NGX_CONF_UNSET_SIZE;
    redis_cf->upconf.connect_timeout = NGX_CONF_UNSET_MSEC;
    redis_cf->upconf.send_timeout = NGX_CONF_UNSET_MSEC;
    redis_cf->upconf.read_timeout = NGX_CONF_UNSET_MSEC;

    /* upstream 模块要求 hide_headers 成员必须初始化 */
    redis_cf->upconf.hide_headers = NGX_CONF_UNSET_PTR;
    redis_cf->upconf.pass_headers = NGX_CONF_UNSET_PTR;

    /* 以下为 upstream_conf_t 硬编码的配置项 */
    /*
     * buffering == 0: 使用固定大小的缓冲去来转发上游响应.这块固定大小的缓冲去由
     *                 buffer_size 决定
     * buffering == 1: 将会使用更多的内存来缓存来不及发往下游的响应.最多使用
     *                 bufs.num 个缓冲区,每个缓冲区大小为 bufs.size.
     *                 另外还会使用临时文件,临时文件的最大长度为:
     *                 max_temp_file_size
     */
    redis_cf->upconf.buffering = 0;
    redis_cf->upconf.bufs.num = 8;
    redis_cf->upconf.bufs.size = ngx_pagesize;
    redis_cf->upconf.buffer_size = ngx_pagesize;
    redis_cf->upconf.busy_buffers_size = 2 * ngx_pagesize;
    redis_cf->upconf.temp_file_write_size = 2 * ngx_pagesize;
    redis_cf->upconf.max_temp_file_size = 1024 * 1024 * 1024;
    redis_cf->nb = NGX_CONF_UNSET_PTR;
    redis_cf->nr = NGX_CONF_UNSET_PTR;
    
    redis_cf->rediskey.rediskey_type = NGX_CONF_UNSET;
    ngx_str_null(&redis_cf->rediskey.rediskey_un.rediskey_conf);
    redis_cf->rediskey.rediskey_un.rediskey_idx = NGX_CONF_UNSET;
    ngx_str_null(&redis_cf->redis_content_type);

    return (void *)redis_cf;
}

static char *
http_redis_conf_set_key(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_redis_module_loc_conf_t    *redis_conf = conf;
    ngx_str_t                           *value = cf->args->elts;

    /* redis-get 接收一个参数, cf->args->elts 应用2个成员(加上自身) */
    if(cf->args->nelts != 2) {
        return NGX_CONF_ERROR;
    }

    if(redis_conf->rediskey.rediskey_type != NGX_CONF_UNSET) {
        return "duplicated conf";
    }
    
    /* 第一个参数的首字母为 $, 则为 nginx 的变量 */
    if(value[1].data[0] == '$') {
        redis_conf->rediskey.rediskey_type = _REDIS_KEY_TYPE_ARG;
        /* 去除第一个字符 */
        value[1].data++;
        value[1].len--;
        /* 获取变量名在 nginx 中的索引值, 加速访问 */
        redis_conf->rediskey.rediskey_un.rediskey_idx = ngx_http_get_variable_index(cf, &value[1]);
        if(redis_conf->rediskey.rediskey_un.rediskey_idx == NGX_ERROR) {
            return NGX_CONF_ERROR;
        }
    } else {
        redis_conf->rediskey.rediskey_type = _REDIS_KEY_TYPE_STR;
        redis_conf->rediskey.rediskey_un.rediskey_conf = value[1];
    }

    return NGX_CONF_OK;
}


static inline char *
_init_buddy_and_cache(ngx_conf_t *cf, ngx_http_redis_module_loc_conf_t *redis_cf)
{
    ngx_buddy_t         *nb;
    ngx_redis_cache_t   *nr;
    void                *ptr;

    if(!redis_cf->cache) {
        return NGX_CONF_OK;
    }
    if(redis_cf->nb != NGX_CONF_UNSET_PTR || redis_cf->nr != NGX_CONF_UNSET_PTR) {
        return NGX_CONF_ERROR;
    }

    nb = ngx_pcalloc(cf->pool, sizeof(ngx_buddy_t) + redis_cf->bsize);
    if(nb == NULL) {
        return NGX_CONF_ERROR;
    }
    /* ngx_log_error(NGX_LOG_ERR, cf->log, 0, "allocated  %z for redis local cache", redis_cf->bsize + sizeof(ngx_buddy_t)); */
    ptr = (u_char *)nb + sizeof(ngx_buddy_t);
    if(ngx_buddy_init(nb, ptr, redis_cf->bsize, _REDIS_BUDDY_SYS_MAGIC) < 0) {
        return NGX_CONF_ERROR;
    }
    nr = ngx_redis_cache_create(cf->pool, redis_cf->hash_bucket_size);
    if(nr == NULL) {
        return NGX_CONF_ERROR;
    }
    redis_cf->nb = nb;
    redis_cf->nr = nr;

    /* ngx_log_error(NGX_LOG_ERR, cf->log, 0, "nb addr: %p; nr addr: %p ", redis_cf->nb, redis_cf->nr); */

    return NGX_CONF_OK;
}

static char *
http_redis_merge_loc_conf(ngx_conf_t *cf, void *prev, void *conf)
{
    ngx_http_redis_module_loc_conf_t     *parent = prev;
    ngx_http_redis_module_loc_conf_t     *current = conf;
    ngx_hash_init_t         hash;
    struct hostent          *redis_host;
    char                    *tmpIpv4DNN, *ipv4DDN;

    hash.max_size = _HASH_MAX_SIZE;
    hash.bucket_size = _HASH_BUCKET_SIZE;
    hash.name = _HASH_NAME;

    if(ngx_http_upstream_hide_headers_hash(cf, &current->upconf, 
                &parent->upconf,
                redis_proxy_hide_headers, &hash) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(current->visit, parent->visit, 0);
    ngx_conf_merge_str_value(current->host, parent->host, 
                             _DEFAULT_REDIS_HOST);
    ngx_conf_merge_value(current->port, parent->port, 
            _DEFAULT_REDIS_PORT);
    ngx_conf_merge_str_value(current->username, parent->username, "");
    ngx_conf_merge_str_value(current->password, parent->password, "");
    ngx_conf_merge_value(current->cache, parent->cache, 0);
    ngx_conf_merge_value(current->cachemiss, parent->cachemiss, 0);
    ngx_conf_merge_msec_value(current->cacheto, parent->cacheto, 
                              _DEFAULT_REDIS_CACHE_TIMEOUT);
    ngx_conf_merge_uint_value(current->hash_bucket_size, parent->hash_bucket_size, _DEFAULT_REDIS_CACHE_BUCKET_SIZE);
    ngx_conf_merge_size_value(current->bsize, parent->bsize, 
                              _DEFAULT_REDIS_BUFF_SIZE);
    ngx_conf_merge_str_value(current->backend, parent->backend, "");
    ngx_conf_merge_msec_value(current->upconf.connect_timeout, 
            parent->upconf.connect_timeout, 
            _DEFAULT_REDIS_CONN_TIMEOUT);
    ngx_conf_merge_msec_value(current->upconf.send_timeout, 
            parent->upconf.send_timeout, 
            _DEFAULT_REDIS_SEND_TIMEOUT);
    ngx_conf_merge_msec_value(current->upconf.read_timeout, 
            parent->upconf.read_timeout, 
            _DEFAULT_REDIS_READ_TIMEOUT);
    ngx_conf_merge_str_value(current->redis_ch, parent->redis_ch, 
            "0");
    ngx_conf_merge_str_value(current->redis_content_type, parent->redis_content_type, 
            _DEFAULT_REDIS_CONTENT_TYPE);
    /* 合并 current->rediskey */
    if(current->rediskey.rediskey_type == NGX_CONF_UNSET) {
        current->rediskey = parent->rediskey;
    }

    /* 没有配置有效的redis地址, 或者不需要访问redis服务 */
    if(current->host.len == 0 || current->visit == 0) {     
        return NGX_CONF_OK;
    }

    /* 以下代码用于获取redis的连接信息 */
    /* WARN: 这个 host.data 不知道是不是用\0结束的字符串 */
    redis_host = gethostbyname((const char*)current->host.data);
    if(redis_host == NULL) {
        return "get redis host by name failed";
    }
    /* WARN: The inet_ntoa() function converts the Internet host address in, given 
     * in network byte order, to  a  string  in  IPv4 dotted-decimal notation.  
     * The string is returned in a statically allocated buffer, 
     * which subsequent calls will **overwrite**.
     */
    tmpIpv4DNN = inet_ntoa(*(struct in_addr *)redis_host->h_addr);
    ipv4DDN = ngx_pcalloc(cf->pool, 16);    /* 255.255.255.255 */
    if(ipv4DDN == NULL) {
        return "ngx_pcalloc failed";
    }
    strcpy(ipv4DDN, tmpIpv4DNN);
    current->backend.data = (u_char *)ipv4DDN;
    current->backend.len = strlen(ipv4DDN);
    /* 初始化 redis_conn->conn 结构体 */
    current->conn.sin_family = AF_INET;  
    current->conn.sin_port = htons((in_port_t)current->port);
    current->conn.sin_addr = *(struct in_addr *)redis_host->h_addr;

    if(current->cache) {
        return _init_buddy_and_cache(cf, current);
    }

    return NGX_CONF_OK;
}


/* 在NGX_HTTP_CONTENT_PHASE 阶段注入处理函数 */
static ngx_int_t 
http_redis_postconfig(ngx_conf_t *cf)
{
    ngx_http_handler_pt                 *h;
    ngx_http_core_main_conf_t           *cmcf;

    /* 注入模块处理函数 */
    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    /* http 模块一般在 NGX_HTTP_CONTENT_PHASE 阶段介入处理请求 */
    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    /* h = ngx_array_push(&cmcf->phases[NGX_HTTP_POST_READ_PHASE].handlers); */
    if(h == NULL) {
        return NGX_ERROR;
    }
    *h = http_redis_module_handler;
    return NGX_OK;
}


/**
 * 初始化 r->upstream 结构体
 * 以及发往上游的请求
 *
 * 这里尝试一下把认证信息和最终的get请求一起发给redis 
 *
 * 从测试程序来看,可以这么做
 *
 * - 认证:
 *   - 第一个字符为 + 表示成功
 *   - 第一个字符为 - 表示失败
 * - GET key:
 *   - $ 正整数: 返回的val长度
 *   - $ -1: 关键字不存在
 */
static ngx_int_t 
http_redis_upstream_create_request(ngx_http_request_t *r)
{
    ngx_http_redis_module_loc_conf_t    *redis_cf;
    redis_module_req_ctx_t              *rctx;
    ngx_buf_t                           *rbuf;
    ngx_int_t                           totallen, querylen;
    off_t                               buf_off = 0;

    redis_cf = ngx_http_get_module_loc_conf(r, ngx_http_redis_module);
    rctx = ngx_http_get_module_ctx(r, ngx_http_redis_module);
    
    totallen = redisAuth.len + redisGet.len + redisSelect.len +  
        redis_cf->redis_ch.len + 
        redis_cf->username.len + redis_cf->password.len + 
        rctx->rediskey.len - 8;   /* %V %V %V %V 8个字符 */
    querylen = totallen;
    rbuf = ngx_create_temp_buf(r->pool, totallen + 1);
    if(rbuf == NULL) {
        goto _error;
    }

    /* redis 需认证 */
    if(redis_cf->password.len != 0) {
        /* AUTH user pass */
        ngx_sprintf(rbuf->pos, (const char *)redisAuth.data, 
                &redis_cf->username, &redis_cf->password);
        /* 修正请求buf的偏移量 */
        buf_off = redisAuth.len + redis_cf->username.len + redis_cf->password.len - 4; /* %V %V 4个字符 */
    } else {
        /* redis 无需认证, 修正请求长度 */
        querylen = redisSelect.len + redisGet.len + rctx->rediskey.len + redis_cf->redis_ch.len - 4; /* %V %V */
    }
    /* SELECT ch */
    ngx_sprintf(rbuf->pos + buf_off, (const char *)redisSelect.data, &redis_cf->redis_ch);
    buf_off += redisSelect.len + redis_cf->redis_ch.len - 2; /* %V */
    /* GET key */
    ngx_sprintf(rbuf->pos + buf_off, (const char *)redisGet.data, &rctx->rediskey);
    rbuf->last = rbuf->pos + querylen;

    r->upstream->request_bufs = ngx_alloc_chain_link(r->pool);
    if(r->upstream->request_bufs == NULL) {
        goto _error;
    }

    r->upstream->request_bufs->buf = rbuf;
    r->upstream->request_bufs->next = NULL;
    r->upstream->request_sent = 0;
    r->upstream->header_sent = 0;
    r->header_hash = 1; /* 不可以为0 */

/* _done: */
    return NGX_OK;

_error:
    return NGX_ERROR;
}

static inline void
_recycle_buddy_mem(redis_module_req_ctx_t *rctx)
{
    ngx_rbtree_node_t                   *rbn;
    ngx_http_redis_module_loc_conf_t    *rlcf = rctx->rlcf;
    ngx_rbtree_t                        *rbt = &rlcf->nr->rbt;
    ngx_redis_cache_node_t              *cn;

    rbn = ngx_rbtree_min(rbt->root, rbt->sentinel);
    while(_msec_now - rbn->key > rlcf->cacheto) {
        cn = (ngx_redis_cache_node_t *)rbn;
        ngx_redis_cache_delete(rlcf->nr, cn);
        ngx_buddy_free(rlcf->nb, cn);
        rbn = ngx_rbtree_min(rbt->root, rbt->sentinel);
    }
}

static inline ngx_redis_cache_node_t *
_init_rcn(redis_module_req_ctx_t *rctx, ngx_str_t *value) 
{
    ngx_http_redis_module_loc_conf_t    *redis_cf = rctx->rlcf;
    ngx_http_request_t                  *r = rctx->request;
    ngx_str_t                           *key = &rctx->rediskey, *v;
    ngx_redis_cache_node_t              *rcn = NULL;
    u_char                              *d;
    ngx_int_t                           tries = 0;

_try_again:
    if(value) {
        rcn = ngx_buddy_alloc(redis_cf->nb, sizeof(ngx_redis_cache_node_t) + key->len + sizeof(ngx_str_t) + value->len);
    } else {
        rcn = ngx_buddy_alloc(redis_cf->nb, sizeof(ngx_redis_cache_node_t) + key->len);
    }
    if(!rcn) {
        if(tries > 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "redis cache NO MEMORY for %V", &rctx->rediskey);
            return NULL;
        }
        tries++;
        _recycle_buddy_mem(rctx);
        goto _try_again;
    }

    d = (u_char *)rcn + sizeof(ngx_redis_cache_node_t);
    ngx_memcpy(d, key->data, key->len);
    rcn->cache.key.data = d;
    rcn->cache.key.len = key->len;
    rcn->cache.value = NULL;
    rcn->cache.key_hash = ngx_redis_cache_hash(&rcn->cache.key);
    ngx_queue_init(&rcn->dlist);
    if(value) {
        v = (ngx_str_t *)(d + key->len);
        v->len = value->len;
        v->data = (u_char *)v + sizeof(ngx_str_t);
        rcn->cache.value = v;
        /* ngx_memcpy(v->data, value->data, value->len); */
    }
    rcn->rbt_node.key = _msec_now;

    return rcn;
}

static inline void 
_redis_cached_miss(redis_module_req_ctx_t *rctx)
{
    ngx_http_redis_module_loc_conf_t    *redis_cf = rctx->rlcf;
    ngx_redis_cache_node_t              *rcn = _init_rcn(rctx, NULL);

    if(!rcn) {
        return; 
    }

    ngx_redis_cache_insert(redis_cf->nr, rcn);
}

/**
 * 分两种情况:
 *   1. 需要认证
 *   2. 不需要认证
 */
static ngx_int_t 
http_redis_upstream_process_header(ngx_http_request_t *r)
{
    ngx_http_upstream_t                 *us;
    redis_module_req_ctx_t              *rctx;
    ngx_http_redis_module_loc_conf_t    *redis_cf; 
    ngx_buf_t                           *b;
    ngx_table_elt_t                     *h;
    ngx_redis_cache_node_t              *rcn;
    ngx_str_t                           v = ngx_null_string;

    /* 获取请求上下文 */
    rctx = ngx_http_get_module_ctx(r, ngx_http_redis_module);

    us = r->upstream;
    b = &us->buffer;
    redis_cf = ngx_http_get_module_loc_conf(r, ngx_http_redis_module);

    switch (rctx->status) {
    case _STATUS_STARTING:
        if(redis_cf->password.len != 0) {
            rctx->status = _STATUS_READING_AUTH;
        } else {
            rctx->status = _STATUS_SELECT_CH;
            goto _select_ch;

        }
        break;
    case _STATUS_READING_AUTH:
        break;
    case _STATUS_SELECT_CH:
        goto _select_ch;
        break;
    case _STATUS_READING_LEN:
        goto _read_len;
        break;
    }

    /* 处理认证信息 */
    _readline(b, &rctx->header, &rctx->hidx);
    rctx->header.data[rctx->hidx] = 0;
    if(rctx->header.data[0] == '-') {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "redis AUTH ERROR: %V", &rctx->header); 
        _set_httpstatus_contenlen(us, 0, NGX_HTTP_UNAUTHORIZED);
        b->pos = b->last;
        return NGX_OK;
    }
    rctx->status = _STATUS_SELECT_CH;

_select_ch:
    _readline(b, &rctx->header, &rctx->hidx);
    rctx->header.data[rctx->hidx] = 0;
    if(rctx->header.data[0] == '-') {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "redis SELECT ERROR: %V", &rctx->header); 
        _set_httpstatus_contenlen(us, 0, NGX_HTTP_NOT_ALLOWED);
        b->pos = b->last;
        return NGX_OK;
    }
    rctx->status = _STATUS_READING_LEN;
    rctx->lenidx = rctx->hidx;

_read_len:
    _readline(b, &rctx->header, &rctx->hidx);
    rctx->header.data[rctx->hidx] = 0;
    switch (rctx->header.data[rctx->lenidx]) {
    case '-':
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "redis NOT AUTH ERROR: %V", &rctx->header); 
        _set_httpstatus_contenlen(us, 0, NGX_HTTP_UNAUTHORIZED);
        b->pos = b->last;
        return NGX_OK;
        break;
    case '$':
        rctx->vallen = atoi((const char *)(rctx->header.data + rctx->lenidx + 1));
        rctx->vallen = (rctx->vallen == -1 ? -2 : rctx->vallen);
        if(rctx->vallen < 0) {
            _set_httpstatus_contenlen(us, 0, NGX_HTTP_NOT_FOUND);
            if(rctx->rlcf->cache && rctx->rlcf->cachemiss) {
                _redis_cached_miss(rctx);
            }
            b->pos = b->last;
            return NGX_OK;
        }
        break;
    default:
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "redis ERROR: unknown value length"); 
        return NGX_ERROR;
    }

    /* 给 upstream->headers_in 赋值 */
    _set_httpstatus_contenlen(us, rctx->vallen, NGX_HTTP_OK);

    if(redis_cf->cache) {   /* 使用本地cache  */
        v.len = rctx->vallen;
        rcn = _init_rcn(rctx, &v);
        rctx->rcn = rcn;
        /* ngx_log_error(NGX_LOG_ERR, rctx->request->connection->log, 0, "allocated node for redis cache : %V", &rcn->cache.key); */
    }

    h = ngx_list_push(&us->headers_in.headers);
    if(h == NULL) {
        return NGX_ERROR;
    }
    h->hash = _hash_str("server");
    ngx_str_set(&h->key, "Server");
    ngx_str_set(&h->value, "redis");
    h->lowcase_key = (u_char *)"server";
       
    h = ngx_list_push(&us->headers_in.headers);
    if(h == NULL) {
        return NGX_ERROR;
    }
    h->hash = _hash_str("content-type");
    ngx_str_set(&h->key, "Content-Type");
    h->value = redis_cf->redis_content_type; /* 从配置中获取响应类型 */
    h->lowcase_key = (u_char *)"content-type";

    /* h = ngx_list_push(&us->headers_in.headers); */
    /* if(h == NULL) { */
    /*     return NGX_ERROR; */
    /* } */
    /* h->hash = _hash_str("connection"); */
    /* ngx_str_set(&h->key, "Connection"); */
    /* ngx_str_set(&h->value, "close"); */
    /* h->lowcase_key = (u_char *)"connection"; */
    us->keepalive = 1;

    return NGX_OK;
}

static void 
http_redis_upstream_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    
    /* ngx_http_redis_module_loc_conf_t *rlcf; */
    /* rlcf = ngx_http_get_module_loc_conf(r, ngx_http_redis_module); */
    /* ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "allocated  %z for redis local cache", rlcf->bsize); */
    /* ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "nb addr: %p; nr addr: %p ", rlcf->nb, rlcf->nr); */

    /* ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,  */
    /*               "redis request end at %M, %ul, %M", ngx_current_msec, _msec_now, _msec_now); */
    return;
}

static ngx_int_t
http_redis_upstream_filter_init(void *data)
{
    redis_module_req_ctx_t  *rctx = data;
    ngx_http_upstream_t     *u;

    u = rctx->upstream;
    u->length = u->headers_in.content_length_n;

    return NGX_OK;
}


static ngx_int_t
http_redis_upstream_filter(void *data, ssize_t bytes)
{
    redis_module_req_ctx_t  *ctx = data;

    ngx_redis_cache_node_t  *rcn = ctx->rcn;
    u_char                  *last;
    ngx_buf_t               *b;
    ngx_chain_t             *cl, **ll;
    ngx_http_upstream_t     *u;
    ngx_str_t               *v;
    off_t                   o;
    size_t                  s;

    u = ctx->upstream;
    b = &u->buffer;

    /* 到链表的末尾 */
    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next;
    }

    cl = ngx_chain_get_free_buf(ctx->request->pool, &u->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf->flush = 1;
    cl->buf->memory = 1;

    *ll = cl;       /* 新分配的buf结构添加到链表末尾 */

    if(rcn) {
        v = rcn->cache.value;
        o = v->len - u->length;
        s = (size_t)(bytes + o) < v->len ? bytes : u->length;
        ngx_memcpy(v->data + o, b->last, s);
        /* ngx_log_error(NGX_LOG_ERR, ctx->request->connection->log, 0, "using redis cache for: %V, %p, %p, %z, %V" ,  */
        /*               &rcn->cache.key, rcn, v, v->len, v); */
    }
    last = b->last;
    cl->buf->pos = last;
    b->last += bytes;
    cl->buf->last = b->last;
    cl->buf->tag = u->output.tag;

    u->length -= bytes;     /* 更新响应体的剩余长度 */
    if(u->length <= 0) {
        u->length = 0;
        if(rcn) {
            ngx_redis_cache_insert(ctx->rlcf->nr, rcn);
        }
        cl->buf->last -= 2; /* 修复 redis \r\n 结束的问题 */
        u->keepalive = 1;
        /* ngx_close_connection(u->peer.connection); */
    }

    return NGX_OK;
}

static inline ngx_redis_cache_node_t *
_check_local_cache(redis_module_req_ctx_t *rctx)
{
    ngx_http_redis_module_loc_conf_t    *redis_cf = rctx->rlcf;
    /* ngx_http_request_t                  *r = rctx->request; */
    ngx_redis_cache_node_t              *rcn = ngx_redis_cache_find(redis_cf->nr, &rctx->rediskey);
    
    if(rcn == NULL) {   /* 没有找到缓存 */
        /* ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "redis cache not found: %V", &rctx->rediskey); */
        return NULL;
    }
    if(_msec_now - rcn->rbt_node.key > redis_cf->cacheto) {     /* 缓存超时 */
        /* ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "redis cache timeout: %V, %M, %M",  */
        /*               &rctx->rediskey, _msec_now, rcn->rbt_node.key); */
        ngx_redis_cache_delete(redis_cf->nr, rcn);
        ngx_buddy_free(redis_cf->nb, rcn);
        return NULL;
    }
    return rcn;
}

static ngx_int_t 
_send_resp_using_cache(ngx_http_request_t *r, ngx_redis_cache_node_t *rcn, ngx_http_redis_module_loc_conf_t *rlcf) 
{
    ngx_str_t   *v = rcn->cache.value;
    ngx_int_t   rc;
    ngx_buf_t   *buf;
    ngx_chain_t *out;

    if(v == NULL) {
        return NGX_HTTP_NOT_FOUND;
    }    


    /* 构建响应头部 */
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = v->len;
    r->headers_out.content_type = rlcf->redis_content_type;
    rc = ngx_http_send_header(r);
    /* 这里 rc > NGX_OK 是个什么情况需要了解下 */
    if( rc == NGX_ERROR || rc > NGX_OK ) {
        return rc;
    }

    /* 构造响应体 */
    /* 不做错误检查 */
    buf = ngx_create_temp_buf(r->pool, v->len);
    if(buf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    buf->last_buf = 1;
    ngx_memcpy(buf->pos, v->data, v->len);
    buf->last = buf->pos + v->len;

    out = ngx_pcalloc(r->pool, sizeof(ngx_chain_t));   /* 不做错误检查 */ 
    if(out == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    out->buf = buf;
    out->next = NULL;

    /* 发送响应体,
     * 发送结束后HTTP框架会调用 ngx_http_finalize_request_t 结束请求 */
    return ngx_http_output_filter(r, out);
}

static ngx_int_t 
http_redis_module_handler(ngx_http_request_t *r)
{
    redis_module_req_ctx_t              *rctx;
    ngx_http_redis_module_loc_conf_t    *redis_cf;
    ngx_http_upstream_t                 *u;
    ngx_http_variable_value_t           *v;
    ngx_redis_cache_node_t              *rcn;
    ngx_int_t                           rc;

    redis_cf = (ngx_http_redis_module_loc_conf_t *)ngx_http_get_module_loc_conf(r, ngx_http_redis_module);

    if(redis_cf->visit == 0) {
        return NGX_DECLINED;
    }

    /* 检查请求方法,如果不是 GET 或者 HEAD 就直接返回 NGX_HTTP_NOT_ALLOW */
    if (!(r->method & NGX_HTTP_GET)) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    /* 创建上下文结构 */
    rctx = ngx_http_get_module_ctx(r, ngx_http_redis_module);
    if(rctx != NULL) {  /* 第二次及以上执行这个函数了 */
        return NGX_DECLINED;
    }
    rctx = _ngx_pcalloc_pool(r->pool, redis_module_req_ctx_t, NGX_ERROR);
    /* 初始化上下文结构 */
    rctx->status = _STATUS_STARTING;
    rctx->header.data = rctx->buf;
    rctx->header.len = 0;
    rctx->hidx = 0;
    rctx->lenidx = 0;
    rctx->request = r;
    rctx->rlcf = redis_cf;
    rctx->rcn = NULL;
    /* 将上下文结构与请求关联起来 */
    ngx_http_set_ctx(r, rctx, ngx_http_redis_module);

    /* 丢弃请求体 */
    if((rc = ngx_http_discard_request_body(r)) != NGX_OK) {
        return rc;
    }

    switch (redis_cf->rediskey.rediskey_type) {
        case _REDIS_KEY_TYPE_ARG:
            v = ngx_http_get_indexed_variable(r, redis_cf->rediskey.rediskey_un.rediskey_idx);
            if(v == NULL || v->not_found) {
                return NGX_HTTP_NOT_FOUND;
            }
            rctx->rediskey.data = v->data;
            rctx->rediskey.len = v->len;
            break;
        case _REDIS_KEY_TYPE_STR:
            rctx->rediskey = redis_cf->rediskey.rediskey_un.rediskey_conf;
            break;
        default:
            return NGX_HTTP_NOT_ALLOWED;
    }
    if(redis_cf->cache) {
        rcn = _check_local_cache(rctx);
        if(rcn) {
            /* ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "redis local cache found for %V", &rctx->rediskey); */
            return _send_resp_using_cache(r, rcn, redis_cf);
        }
        /* } else { */
        /*     ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "NO redis local cache found for %V", &rctx->rediskey); */
        /* } */
    }
    
    /* 对每一个使用 upstream 请求,必须调用且调用1次 ngx_http_upstream_create,
     * 它会初始化 r->upstream 成员 */
    if(ngx_http_upstream_create(r) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "create upstream failed.");
        return NGX_ERROR;
    }

    u = r->upstream;
    rctx->request = r;
    rctx->upstream = u;

    /* 给 upstream 成员赋值 */
    u->conf = &redis_cf->upconf;
    u->buffering = redis_cf->upconf.buffering;   /* 使用固定缓存区大小转发请求 */

    /* 初始化 u->resolved 结构 */
    u->resolved = _ngx_pcalloc_pool(r->pool, ngx_http_upstream_resolved_t, NGX_ERROR);
    u->resolved->sockaddr = (struct sockaddr *)&redis_cf->conn;
    u->resolved->port = (in_port_t)redis_cf->port;
    u->resolved->host = redis_cf->backend;
    u->resolved->socklen = sizeof(struct sockaddr_in);
    u->resolved->naddrs = 1;

    /* upstream 中3个重要的回调函数 */
    u->create_request = http_redis_upstream_create_request;
    u->process_header = http_redis_upstream_process_header;
    u->finalize_request = http_redis_upstream_finalize_request;
    /* r->subrequest_in_memory = 1; */
    u->input_filter_ctx = rctx;
    u->input_filter_init = http_redis_upstream_filter_init;
    u->input_filter = http_redis_upstream_filter;

    r->main->count++;   /* 请求数加1 */

    /* 启动 upstream */
    ngx_http_upstream_init(r);

    /* 必须返回 NGX_DONE */
    return NGX_DONE;
}

