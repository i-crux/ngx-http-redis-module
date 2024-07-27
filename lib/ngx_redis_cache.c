#include <ngx_redis_cache.h>
#include <ngx_redis_common.h>

extern volatile ngx_time_t      *ngx_cached_time;


ngx_redis_cache_t *
ngx_redis_cache_create(ngx_pool_t *pool, ngx_uint_t bucket_size)
{
    ngx_redis_cache_t   *nr;
    ngx_queue_t         *b;
    ngx_uint_t          i;

    if((nr = ngx_pcalloc(pool, sizeof(ngx_redis_cache_t) + sizeof(ngx_queue_t) * bucket_size)) == NULL) {
        return NULL;
    }
    b =(ngx_queue_t *)((u_char *)nr + sizeof(ngx_redis_cache_t));
   
    nr->bucket_size = bucket_size;
    nr->buckets = b; 

    /* 初始化列表头 */
    for(i = 0; i < bucket_size; i++) {
        ngx_queue_init(b+i);
    }
    /* 初始化红黑树 */
    ngx_rbtree_init(&nr->rbt, &nr->rbt_sentinel, ngx_rbtree_insert_value);

    return nr;
}


/* 本函数不管理任何内存 */
void 
ngx_redis_cache_node_init(ngx_redis_cache_node_t *rcn, ngx_str_t *key, void *value)
{
    ngx_msec_t  msec;

    rcn->cache.key.data = key->data;
    rcn->cache.key.len = key->len;
    rcn->cache.value = value;
    rcn->cache.key_hash = ngx_redis_cache_hash(key);
    ngx_queue_init(&rcn->dlist);
    msec = ngx_cached_time->msec + 1000 * ngx_cached_time->sec;
    rcn->rbt_node.key = msec;
}


void 
ngx_redis_cache_insert(ngx_redis_cache_t *nr, ngx_redis_cache_node_t *rcn)
{
    ngx_uint_t idx = rcn->cache.key_hash % nr->bucket_size;

    ngx_queue_insert_head(nr->buckets + idx, &rcn->dlist);
    ngx_rbtree_insert(&nr->rbt, &rcn->rbt_node);
}


ngx_redis_cache_node_t *
ngx_redis_cache_find(ngx_redis_cache_t *nr, ngx_str_t *key)
{
    ngx_uint_t              idx = ngx_redis_cache_hash(key) % nr->bucket_size;
    ngx_queue_t             *h = nr->buckets + idx, *l;
    ngx_redis_cache_node_t  *nrc;
    
    l = h;
    while((l = ngx_queue_next(l)) != ngx_queue_sentinel(h)) {
        nrc = ngx_queue_data(l, ngx_redis_cache_node_t, dlist);
        if(_ngx_str_t_casecmp(key, &nrc->cache.key) == 0) {
            return nrc;
        }
    }

    return NULL;
}


void 
ngx_redis_cache_delete(ngx_redis_cache_t *nr, ngx_redis_cache_node_t *rcn)
{
    ngx_queue_remove(&rcn->dlist);
    ngx_rbtree_delete(&nr->rbt, &rcn->rbt_node);
}

