#include "ngx_buddy.h"


/* 定义常量 */
/* 伙伴系统管理块使用的内存 64 */
#define BUDDY_MGR_BLOCK_SIZE 0x40UL
/* 对齐用的常数 */
#define BUDDY_ALIGN         0xffffffffffffff00UL


/* 定义数据类型 */
typedef struct buddy_mgr_node_s {
    uint32_t        magic;      /* 32 bits */
    uint32_t        order;      /* 32 bits */
    ngx_queue_t     dlist;      /* 128 bits */
} buddy_mgr_node_t;


/* 宏函数 */
/* 地址向上对齐 */
#define _ceiladdr(a)   \
    ( ((a) & (~BUDDY_ALIGN)) ? (((a) & BUDDY_ALIGN) + BUDDY_MIN_MEM_SIZE) : (a))

/* 地址向下对齐 */
#define _flooraddr(a)  ((a) & BUDDY_ALIGN)

/* 判断该地址是否可以用于合并: 该地址是合并后的大小的整数倍*/
#define _addr_can_uion(a, o) \
  ( ((a) % ((1 << ((o) + 1)) * BUDDY_MIN_MEM_SIZE)) ? 0 : 1 )

/* 找到地址的伙伴地址 */
#define _find_buddy(a, o) \
  ( (a) + (BUDDY_MIN_MEM_SIZE << (o)) )

/* 判断找到的buddy是否可以用于合并
 * 1. 存在魔术
 * 2. 阶相等
 * 3. 头节点正确 
 */
#define _unused_buddy(a, o, m)                  \
  ( ((buddy_mgr_node_t *)(a))->magic == (m) &&  \
    ((buddy_mgr_node_t *)(a))->order == (o) )

/* 初始化 buddy_mgr_node_t */
#define _init_buddy_mgr_node(p, o, m)                       \
    do {                                                    \
        buddy_mgr_node_t    *_bmn = (buddy_mgr_node_t *)(p);\
        _bmn->magic = (m); _bmn->order = (o);               \
        ngx_queue_init(&_bmn->dlist);                       \
    } while(0)

/* 通过order 获取buddy的大小 */
#define _get_size_by_order(o)   ((1 << (o)) * BUDDY_MIN_MEM_SIZE)



/* 静态函数定义 */
/* 通过size得到order */
static inline uint32_t  
_get_order_by_size(size_t size)
{
    uint32_t order, cnt = size / BUDDY_MIN_MEM_SIZE, mask = 0xffffffffU;;

    cnt = (size % BUDDY_MIN_MEM_SIZE) ? (cnt + 1) : cnt;
    asm volatile ("bsr %k1, %k0" : "=a"(order) : "b"(cnt) : "cc", "memory"); 
    /* printf("cnt: %u, order: %u\n", cnt, order); */
    if(order != 0) {
        mask >>= (sizeof(uint32_t) * 8 - order);
        /* printf("mask = %u\n", mask); */
        order = (cnt & mask) ? (order + 1) : (order);
    }
    return order;
}


static inline void
_union_buddy(ngx_buddy_t *nb, uint64_t addr, uint32_t order)
{
    uint64_t            buddy_addr;
    buddy_mgr_node_t    *bm_node, *bm_node_buddy;

    if(order >= BUDDY_MAX_ORDER) {
        return;     /* 超出最大order, 不做处理 */
    }
    
    if(!_addr_can_uion(addr, order)) {
        return;     /* 地址不可以合并 */
    }
    buddy_addr = _find_buddy(addr, order);
    if(buddy_addr >= nb->start + nb->size) {
        return;     /* 内存越界 */
    }
    if(!_unused_buddy(buddy_addr, order, nb->magic)) {
        return;     /* 不存在可用的伙伴内存 */
    }
    
    bm_node = (buddy_mgr_node_t *)addr;
    bm_node_buddy = (buddy_mgr_node_t *)buddy_addr;
    /* 从原来的链表中删除节点 */
    ngx_queue_remove(&bm_node_buddy->dlist);
    ngx_queue_remove(&bm_node->dlist);
    bzero((void *)bm_node_buddy, sizeof(buddy_mgr_node_t));
    /* 完成合并 */
    bm_node->order++;
    ngx_queue_insert_head(&nb->array[order+1], &bm_node->dlist);
}

static inline void
_union_buddy_to_order(ngx_buddy_t *nb, uint32_t order)
{
    buddy_mgr_node_t    *bm_node;
    ngx_queue_t         *head, *bmh, tmphead;
    uint32_t            o;
    uint64_t            addr;
    
    if(order > BUDDY_MAX_ORDER) {
        return;     /* 超过最大order */
    }

    for(o = 0; o < order; o++) {
        ngx_queue_init(&tmphead);
        head = &nb->array[o];
        while(!ngx_queue_empty(head)) {
            bmh = ngx_queue_head(head);     /* 获取第一个元素 */
            ngx_queue_remove(bmh);          /* 从原链表中删除 */
            ngx_queue_insert_head(&tmphead, bmh);   /* 插入到临时链表中 */
            bm_node = ngx_queue_data(bmh, buddy_mgr_node_t, dlist);
            addr = (uint64_t)bm_node;       /* 获取地址值 */
            _union_buddy(nb, addr, o);      /* 合并伙伴 */
        }
        if(!ngx_queue_empty(&tmphead)) {    /* 如果临时链表非空, 则修正现有链表 */
            tmphead.next->prev = head;
            tmphead.prev->next = head;
            head->next = tmphead.next;
            head->prev = tmphead.prev;
        }
    }
}

static inline void
_split_buddy(ngx_buddy_t *nb, uint32_t o) 
{
    buddy_mgr_node_t    *bm_node, *buddy_bm_node;
    uint32_t            order = o - 1;
    size_t              s;
    ngx_queue_t         *h, *l;

    if(o == 0 || ngx_queue_empty(&nb->array[o])) { /* 不能再分裂 */
        return;
    }
     
    l = ngx_queue_head(&nb->array[o]);      /* 拿到第一个块 */
    bm_node = ngx_queue_data(l, buddy_mgr_node_t, dlist);
    ngx_queue_remove(l);                    /* 从原来的链表中删除节点 */
    s = _get_size_by_order(order);
    buddy_bm_node = (buddy_mgr_node_t *)((u_char *)bm_node + s);
    buddy_bm_node->order = bm_node->order = order;
    buddy_bm_node->magic = bm_node->magic;
    h = &nb->array[order];
    ngx_queue_insert_head(h, &bm_node->dlist);
    ngx_queue_insert_head(h, &buddy_bm_node->dlist);
}

static inline void
_split_buddy_from_order_to_order(ngx_buddy_t *nb, uint32_t fo, uint32_t to)
{
    uint32_t        order;

    if(fo > BUDDY_MAX_ORDER || fo <= to) {    /* 参数非法 */
        return;
    }
    
    if(ngx_queue_empty(&nb->array[fo])) {   /* 无空闲内存 */
        return;
    }
    
    for(order = fo; order > to; order--) {
        _split_buddy(nb, order);
    } 
}

/* 函数体定义 */
ngx_int_t   
ngx_buddy_init(ngx_buddy_t *nb, void *ptr, size_t s, uint32_t magic)
{
    uint64_t            ptr_begin = (uint64_t) ptr, ptr_end;
    uint64_t            addr, addr_begin, addr_end;
    buddy_mgr_node_t    *bm_node;
    size_t              mem_size;
    uint32_t            order;

    if(!magic || s >= BUDDY_MAX_MEM_SIZE) {
        return -1;  /* 魔数为0, 或者内存超大 */
    }

    ptr_end = ptr_begin + s;
    addr_begin = _ceiladdr(ptr_begin);
    addr_end = _flooraddr(ptr_end);
    mem_size = addr_end - addr_begin;

    if (addr_begin >= addr_end) {
        return -2;  /* 对齐之后没有多余的内存了 */
    }

    /* 初始化 ba */
    nb->magic = magic;
    nb->block_start = ptr_begin;
    nb->block_size = s;
    nb->start = addr_begin;
    nb->size = mem_size;
    /* 初始化array数组 */
    for(order = 0; order <= BUDDY_MAX_ORDER; order++) {
        ngx_queue_init(&nb->array[order]);
    }

    /* 把所有的内存都放到order = 0 的链表中, 然后在调用 _union_buddy 合并伙伴 */
    for(addr = addr_begin; addr < addr_end; addr += BUDDY_MIN_MEM_SIZE) {
        bm_node = (buddy_mgr_node_t *)addr; 
        _init_buddy_mgr_node(addr, 0, nb->magic);
        ngx_queue_insert_head(&nb->array[0], &bm_node->dlist);
        /* printf("self: %p; magic: %u; order: %u; prev: %p; next: %p\n", bm_node,  */
        /*        bm_node->magic, bm_node->order, bm_node->dlist.prev, bm_node->dlist.next); */
    }
    _union_buddy_to_order(nb, BUDDY_MAX_ORDER);

    /* ngx_queue_t *h, *l; */
    /* for(order = 0; order <= BUDDY_MAX_ORDER; order++) { */
    /*     h = &nb->array[order]; */
    /*     l = ngx_queue_next(h); */
    /*     while(l != h) { */
    /*         bm_node = ngx_queue_data(l, buddy_mgr_node_t, dlist); */
    /*         printf("order loop: %u, order: %u, magic: %u, bm_node:%p\n",  */
    /*                order, bm_node->order, bm_node->magic, bm_node); */
    /*         l = ngx_queue_next(l); */
    /*     } */
    /* } */

    return 0;
}


void *
ngx_buddy_alloc(ngx_buddy_t *nb, size_t s)
{
    uint32_t            torder, forder;
    uint32_t            trytime = 0;
    ngx_queue_t         *h, *l;
    buddy_mgr_node_t    *bmn_node;
    size_t              realsize;
    void                *ret;

    if(s >= BUDDY_MAX_MEM_SIZE) {   /* 内存超过限制 */
        return NULL;
    }
    s += BUDDY_MGR_BLOCK_SIZE;
    torder = _get_order_by_size(s);
    /* printf("size: %lu, order: %u\n", s, torder); */
    h = &nb->array[torder];

_try_again:
    for(forder = torder; forder <= BUDDY_MAX_ORDER; forder++) {
        if(!ngx_queue_empty(&nb->array[forder])) {
            break;
        }
    }
    _split_buddy_from_order_to_order(nb, forder, torder);
    if(ngx_queue_empty(h)) {
        if(trytime > 0) {
            goto _no_mem;
        }
        _union_buddy_to_order(nb, torder);
        trytime++;
        goto _try_again;
    } else {
        l = ngx_queue_head(h);
        ngx_queue_remove(l);
        bmn_node = ngx_queue_data(l, buddy_mgr_node_t, dlist);
        realsize = _get_size_by_order(torder);
        bzero((void *)bmn_node, sizeof(realsize));
        bmn_node->magic = 0;
        bmn_node->order = torder;
        ret = (void *)((u_char *)bmn_node + BUDDY_MGR_BLOCK_SIZE);
        /* printf("size: %lu, order: %u, addr_ori: %p, addr: %p\n",  */
        /*        s, torder, bmn_node, ret); */
        return ret;
    }

_no_mem:
    return NULL;
}


void        
ngx_buddy_free(ngx_buddy_t *nb, void *ptr)
{
    buddy_mgr_node_t *bm_node = (buddy_mgr_node_t *)((u_char *) ptr - BUDDY_MGR_BLOCK_SIZE);

    if(bm_node->order > BUDDY_MAX_ORDER || bm_node->magic != 0) {
        return;
    } 

    bm_node->magic = nb->magic;
    ngx_queue_insert_head(&nb->array[bm_node->order], &bm_node->dlist);
}
