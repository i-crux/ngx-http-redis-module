#ifndef _NGX_REDIS_COMMON_H_
#define _NGX_REDIS_COMMON_H_

#define _ngx_str_t_casecmp(ngx_str1, ngx_str2) ({                           \
        ngx_int_t _rc = (ngx_str1)->len - (ngx_str2)->len;                  \
        _rc = _rc == 0 ? ngx_strncasecmp((ngx_str1)->data, (ngx_str2)->data,\
                (ngx_str1)->len) : _rc;                                     \
        _rc; })

#define _ngx_pcalloc_pool(pool, type, reterr) ({                \
            type *_ptr;                                         \
            if( !(_ptr = ngx_pcalloc((pool), sizeof(type))) ) { \
                return (reterr);                                \
            }                                                   \
            _ptr;                                               \
        })

#endif
