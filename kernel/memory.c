#include "memory.h"
#include "thread.h"
#include "sync.h"
#include "interrupt.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "global.h"
#include "debug.h"
#include "string.h"
#include "io.h"

#define PG_SIZE 4096
#define NULL 0

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
	struct lock lock;
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

	lock_init(&kernel_pool.lock);
	lock_init(&user_pool.lock);
}


void mem_init(){
	put_str("mem_init  start\n");
	uint32_t mem_bytes_total = (*(uint32_t *)(0xb06));
	mem_pool_init(mem_bytes_total); //初始化内存池
	//intr_enable();
	//while(1);
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
		//用户内存池
		struct task_struct *cur = running_thread();
		bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);
		if(bit_idx_start == -1){
			return NULL;
		}
		while(cnt < pg_cnt){
			bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
		}
		vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;

		ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));

	}
	return (void *)vaddr_start;
}

/*得到虚拟地址vaddr对应的pte指针*/
//只是根据虚拟地址转换规则计算出来vaddr相应的pte及pde地址，与其是否存在无关
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
	if(bit_idx == -1){
		return NULL;
	}
	bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
	uint32_t page_phyaddr = ((bit_idx * PG_SIZE ) + m_pool->phy_addr_start);
	return (void *)page_phyaddr;
}

/*页表中添加虚拟地址和物理地址的映射*/
static void page_table_add(void *_vaddr, void *_page_phyaddr){
	uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
	uint32_t *pde = pde_ptr(vaddr);
	uint32_t *pte = pte_ptr(vaddr);
/*****************************************************************
 * 执行*pte，会访问到空的pde。所以确保pde创建完成之后才能执行*pte
 * 否则会引发page_fault.
 * *pte只能 出现在*pde后面***************************************
 ****************************************************************/
	//现在页目录内判断目录项的P位，若为1，则该表已存在
	if(*pde & 0x00000001){
		ASSERT(!(*pte & 0x00000001));
		//应在不存在页表时创建页表
		if(!(*pte & 0x00000001)){
			*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
		}else{
			PANIC("pte repeat");
			*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
		}
	}else{	//页目录项不存在
		uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
		*pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
		/*分配到的页表地址应该被清0，防止旧数据成了页表项，让页表项混乱*/
		memset((void *)((int)pte & 0xfffff000), 0, PG_SIZE );
		ASSERT(!(*pte & 0x00000001));
		*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
	}
}

/*分配pg_cnt个页空间，成功则返回虚拟地址，失败返回NULL*/
void *malloc_page(enum pool_flags pf, uint32_t pg_cnt){
	ASSERT(pg_cnt > 0 && pg_cnt < 3840);
	/************************malloc_page原理)*******************************
	 * 1.通过vaddr_get获得相应的虚拟地址************************************
	 * 2.通过palloc在物理内存池中获得物理页*********************************
	 * 3.通过table_page_add将得到的虚拟地址和物理地址在页表完成映射*********
	 * ********************************************************************/
	void *vaddr_start = vaddr_get(pf, pg_cnt);
	if(vaddr_start == NULL) return NULL;

	uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
	struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
	
	/*虚拟地址是连续的，物理地址可能不是，所以做逐个映射*/
	while(cnt-- > 0){
		void *page_phyaddr = palloc(mem_pool);
		if(page_phyaddr == NULL){
			//pass 物理页需要全部回滚
			return NULL;
		}
		page_table_add((void*)vaddr, page_phyaddr);
		vaddr += PG_SIZE;
	}
	return vaddr_start;
}


/*从内核内存池申请1页内存*/
void *get_kernel_pages(uint32_t pg_cnt){
	void *vaddr = malloc_page(PF_KERNEL, pg_cnt);
	if(vaddr != NULL){
		memset(vaddr, 0, pg_cnt * PG_SIZE);
	}
	/*
	put_str("vaddr: ");
	put_int(vaddr);
	put_str("\n");
	*/
	return vaddr;
}

/*在用户空间申请内存*/
void *get_user_pages(uint32_t pg_cnt){
	lock_acquire(&user_pool.lock);
	void *vaddr = malloc_page(PF_USER, pg_cnt);
	memset(vaddr, 0 , pg_cnt * PG_SIZE);
	lock_release(&user_pool.lock);
	return vaddr;
}

/*将地址vaddr与pf池中的物理地址关联，仅支持一页空间分配*/
void *get_a_page(enum pool_flags pf, uint32_t vaddr){
	struct pool *mem_pool = pf &PF_KERNEL  ?  &kernel_pool : &user_pool;
	lock_acquire(&mem_pool->lock);

	/*将虚拟地址对应的位图置1*/
	struct task_struct *cur = running_thread();
	int32_t bit_idx = -1;

	/*若当前用户进程申请内存，就修改用户自己的虚拟地址位图*/
	if(cur->pgdir != NULL && pf == PF_USER){
		bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
		ASSERT(bit_idx > 0);
		bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
	}else if(cur->pgdir == NULL && pf == PF_KERNEL){
		bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
		ASSERT(bit_idx > 0);
		bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
	}else{
		PANIC("get_a_page: not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
	}
		
	void *page_phyaddr = palloc(mem_pool);
	if(page_phyaddr == NULL){
		return NULL;
	}
	page_table_add((void*)vaddr, page_phyaddr);
	lock_release(&mem_pool->lock);
	return (void*)vaddr;
}

/*得到虚拟地址映射到物理地址*/
uint32_t addr_v2p(uint32_t vaddr){
	uint32_t *pte = pte_ptr(vaddr);
	return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}


