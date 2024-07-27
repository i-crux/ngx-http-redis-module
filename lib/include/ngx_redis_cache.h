#ifndef _NGX_REDIS_CACHE_H_
#define _NGX_REDIS_CACHE_H_

#include <ngx_hash.h>
#include <ngx_rbtree.h>
#include <ngx_queue.h>

/* 定义数据结构 */
typedef struct ngx_redis_cache_node_s {
    ngx_rbtree_node_t   rbt_node;
    ngx_hash_key_t      cache;
    ngx_queue_t         dlist;
} ngx_redis_cache_node_t;


typedef struct ngx_redis_cache_s {
    ngx_uint_t          bucket_size;
    ngx_queue_t         *buckets;
    ngx_rbtree_t        rbt;
    ngx_rbtree_node_t   rbt_sentinel;
} ngx_redis_cache_t;

/* 宏函数 */
#define ngx_redis_cache_hash(str) ({                                    \
    size_t _l  = (str)->len;                                            \
    u_char *_c = (str)->data;                                           \
    ngx_uint_t  _hash = 0;                                              \
    while(_l > 0) {                                                     \
        _hash = ngx_hash(_hash, *_c);                                   \
        _c++;                                                           \
        _l--;                                                           \
    }                                                                   \
    _hash;                                                              \
})


/* 函数原型 */
ngx_redis_cache_t       *ngx_redis_cache_create(ngx_pool_t *pool, ngx_uint_t bucket_size);
void                    ngx_redis_cache_node_init(ngx_redis_cache_node_t *rcn, ngx_str_t *key, void *value);
void                    ngx_redis_cache_insert(ngx_redis_cache_t *nr, ngx_redis_cache_node_t *rcn);
ngx_redis_cache_node_t  *ngx_redis_cache_find(ngx_redis_cache_t *nr, ngx_str_t *key);
void                    ngx_redis_cache_delete(ngx_redis_cache_t *nr, ngx_redis_cache_node_t *rcn);


#endif
