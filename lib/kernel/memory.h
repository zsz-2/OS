#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#include "list.h"

/*虚拟地址池，用于虚拟地址的管理*/
struct virtual_addr{
	struct bitmap vaddr_bitmap;
	uint32_t vaddr_start;
};

extern struct pool kernel_pool,user_pool;
void mem_init(void);
void *get_kernel_pages(uint32_t pg_cnt);

/*用来判断采用哪个内存池*/
enum pool_flags{
	PF_KERNEL = 1, //内核内存池
	PF_USER = 2  //用户内存池
};

#define PG_P_1 1  //页表项和页目录存在属性为
#define PG_P_0 0
#define PG_RW_R 0  //R/W属性位值，读/执行
#define PG_RW_W 2  //R/W属性位值，读/写/执行
#define PG_US_S 0  //U/S属性位值，系统级
#define PG_US_U 4  //U/S属性位值，用户级

/*内存块*/
struct mem_block{
	struct list_elem free_elem;
};

/*内存块描述符*/
struct mem_block_desc{
	uint32_t block_size;  //内存块大小
	uint32_t block_per_arena; //本arena中可容纳此mem_block的数量
	struct list free_list;
};
void block_desc_init(struct mem_block_desc *);
void sys_free(void*);
void *sys_malloc(uint32_t size);
#define DESC_CNT 7 //内存块描述符个数

#endif
