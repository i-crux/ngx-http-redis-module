#ifndef _NGX_BUDDY_H_
#define _NGX_BUDDY_H_

#include <ngx_config.h>
#include <ngx_queue.h>


/* 定义常量 */
/* 伙伴系统最大阶，结合最小分配单位，伙伴系统的最大内存分配大小为4G */
#define BUDDY_MAX_ORDER     24      
/* 伙伴系统最小分配内存，单位字节 256 */
#define BUDDY_MIN_MEM_SIZE  0x100UL
/* 伙伴系统最大分配的内存 */
#define BUDDY_MAX_MEM_SIZE  ((1 << BUDDY_MAX_ORDER) * BUDDY_MIN_MEM_SIZE)


/* 定义数据类型 */
typedef struct ngx_buddy_s {
    uint32_t        magic;          /* 这个伙伴系统的魔数, 用于标识内存属于该伙伴系统 */
    uint64_t        block_start;    /* 整块内存的起始地址 */
    size_t          block_size;     /* 整块内存的大小 */
    uint64_t        start;          /* 对齐后用于伙伴系统的起始地址 */
    size_t          size;           /* 对齐后用于伙伴系统的大小 */
    ngx_queue_t     array[BUDDY_MAX_ORDER + 1];
} ngx_buddy_t;


/* 函数原型 */
/**
 * @param ba: 伙伴系统链表array
 * @param ptr: 整块内存的起始地址
 * @param s: ptr 所指向的内存大小
 * @param magic: 魔数, 不能为0
 * 
 * @return: 0: 成功; < 0: 失败
 */
ngx_int_t   ngx_buddy_init(ngx_buddy_t *nb, void *ptr, size_t s, uint32_t magic);

/**
 * @param ba: 伙伴系统链表array
 * @param s: 这次要分配的大小
 *
 * @return: 成功: 指向可用内存的指针; 失败: NULL
 */
void        *ngx_buddy_alloc(ngx_buddy_t *nb, size_t s);

/**
 * @param ba: 伙伴系统链表array
 * @param ptr: 被是否内存的首地址
 *
 * @return: NULL
 */
void        ngx_buddy_free(ngx_buddy_t *nb, void *ptr);

#endif
