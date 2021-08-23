#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "global.h"
#include "debug.h"
#include "string.h"

#define PG_SIZE 4096

/*********************************位图地址***********************************
 * 因为0xc009f000是内核线程的栈顶，0xc0009e000是内核主线程的pcb
 * 一个页框大小的位图可表示128MB内存，位图安排在0xc009a000
 * 本系统支持4个页框的位图，即512MB*/

#define MEM_BITMAP_BASE 0xc009a000
/***************************************************************************/


/*0xc0000000表示内核从虚拟地址3G起。0x100000表示跨过低端1MB内存*/
#define K_HEAP_START 0xc0100000

/*获得页目录项索引和页表索引*/
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/*内存池结构，生成两个实例用于管理内核内存池和用户内存池*/
struct pool{
	struct bitmap pool_bitmap; //内存池管理内存的位图结构
	uint32_t phy_addr_start;   //内存池起始的物理地址
	uint32_t pool_size;	   //物理内存池的容量(虚拟地址内存池可以视为无限的)
};

struct pool kernel_pool, user_pool; //生成内核内存池和用户内存池
struct virtual_addr kernel_vaddr;   //此结构用来给内核分配虚拟地址

static void mem_pool_init(uint32_t all_mem){
	put_str("   mem_pool_init start\n");
	//用来记录页目录项和页表占用的字节大小(769~1022个目录项指向254个页表，0和768指向同一个页表)
	uint32_t page_table_size = PG_SIZE * 256;	
	//已经使用的内存，页表和低端1MB内存
	uint32_t used_mem = page_table_size + 0x100000;
	uint32_t free_mem = all_mem - used_mem;
	//对于以页为单位的内存分配策略，不足1页的内存无需考虑
	uint16_t all_free_pages = free_mem / PG_SIZE;

	uint16_t kernel_free_pages = all_free_pages / 2;
	uint16_t user_free_pages = all_free_pages - kernel_free_pages;

	/*为了简化操作，内存不再处理*/
	uint32_t kbm_length  = kernel_free_pages / 8;
	uint32_t ubm_length = user_free_pages / 8;

	//内核内存池的起始地址
	uint32_t kp_start = used_mem;
	//用户内存池的起始地址
	uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;

	kernel_pool.phy_addr_start = kp_start;
	user_pool.phy_addr_start = up_start;

	kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
	user_pool.pool_size = user_free_pages * PG_SIZE;

	kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
	user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

/**************************内核内存池和用户内存池位图***********************
 * 全局或者静态数组在编译时要知道其长度
 * 我们需要根据总内存大小算出需要多少字节
 * 所以指定一块内存来生成位图
 * ************************************************************************/
	//内核位图
	kernel_pool.pool_bitmap.bits = (void *) MEM_BITMAP_BASE;
	//用户位图
	user_pool.pool_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length);

	put_str("      kernel_pool_bitmap_start:");
	put_int((int)kernel_pool.pool_bitmap.bits);
	put_str("      kernel_pool_phy_addr_start:");
	put_int((int)kernel_pool.phy_addr_start);
	put_str("\n");

	put_str("      user_pool_bitmap_start:");
	put_int((int)user_pool.pool_bitmap.bits);
	put_str("      user_pool_phy_addr_start:");
	put_int((int)user_pool.phy_addr_start);
	put_str("\n");

	/*位图置0*/
	bitmap_init(&kernel_pool.pool_bitmap);
	bitmap_init(&user_pool.pool_bitmap);


	/*初始化内核虚拟地址的位图,要和内核内存池大小一致*/
	kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
	/*位图数组指向一块未使用的内存*/
	kernel_vaddr.vaddr_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length + ubm_length);

	kernel_vaddr.vaddr_start = K_HEAP_START;
	bitmap_init(&kernel_vaddr.vaddr_bitmap);
	put_str("  mem_pool_init done\n");
}


void mem_init(){
	put_str("mem_init  start\n");
	uint32_t mem_bytes_total = (*(uint32_t *)(0xb06));
	mem_pool_init(mem_bytes_total); //初始化内存池
	put_str("mem_init  done\n");
}


/*在pf表示的虚拟内存池中申请pg_cnt个虚拟页*
 * 成功则返回虚拟页的起始地址，失败返回NULL*/
static void *vaddr_get(enum pool_flags pf, uint32_t pg_cnt){
	int vaddr_start = 0, bit_idx_start = -1;
	uint32_t cnt = 0;
	if(pf == PF_KERNEL){
		bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
		if(bit_idx_start == -1){
			return NULL;
		}
		while(cnt < pg_cnt){
			bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
		}
		vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
	}else{
	}
	return (void *)vaddr_start;
}

/*得到虚拟地址vaddr对应的pte指针*/
uint32_t *pte_ptr(uint32_t vaddr){
	/*先访问到页表自己，再用页目录项pde作为pte的索引访问到页表*/
	uint32_t *pte = (uint32_t *)(0xffc00000 + ((vaddr & 0xffc00000) >> 10 ) + PTE_IDX(vaddr) * 4);
	return pte;
}

/*得到虚拟地址vaddr对应的pde指针*/
uint32_t *pde_ptr(uint32_t vaddr){
	uint32_t *pde = (uint32_t *)((0xfffff000) + PDE_IDX(vaddr) * 4);
	return pde;
}

/*在m_pool指向的物理内存池中分配一个物理页，成功则返回页框的物理地址，失败则返回NULL*/
static void *palloc(struct pool *m_pool){
	/*扫描设置位图要保证原子操作*/
	int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1); //找一个物理页面
	if(bit_dix == -1){
		return NULL;
	}
	bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
	uint32_t page_phyaddr = ((bit_idx * PG_SIZE ) + m_pool->phy_addr_start);
	return (void *)page_phyaddr;
}
