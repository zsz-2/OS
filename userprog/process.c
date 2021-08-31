#include "global.h"
#include "debug.h"
#include "userprog.h"
#include "list.h"
#include "thread.h"
#include "io.h"
#include "stdint.h"

extern void intr_exit(void);

extern struct list thread_ready_list; //就绪队列
extern struct list thread_all_list; //所有任务队列

/*构建用户进程初始上下文信息*/
void start_process(void *filename_){
	
	intr_enable();
	void *function = filename_;
	struct task_struct *cur = running_thread();
	cur->self_kstack += sizeof(struct thread_stack);
	struct intr_stack *proc_stack = (struct intr_stack *)cur->self_kstack;
	proc_stack->edi = proc_stack->esi =\
		proc_stack->ebp = proc_stack->esp_dummy = 0;
	proc_stack->ebx = proc_stack->edx = \
		proc_stack->ecx = proc_stack->eax = 0;
	proc_stack->gs = 0;   //用户态用不上，直接初始化为0
	proc_stack->ds = proc_stack->es = \
		proc_stack->fs = SELECTOR_U_DATA;
	proc_stack->eip = function;
	proc_stack->cs =  SELECTOR_U_CODE;
	proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
	proc_stack->esp = (void *)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);
	proc_stack->ss = SELECTOR_U_DATA;
//	put_str("error\n");
	//while(1);
	asm volatile("movl %0, %%esp; jmp intr_exit" \
			::"g"(proc_stack):"memory");

}

/*激活页表*/
void page_dir_activate(struct task_struct *pthread){
/*********************************************************************
 * 执行此函数时，当前任务可能是线程
 * 之所以对线程也要安装页表，是因为上一次被调度的可能是进程
 ********************************************************************/

/*若为内核线程，需要重新填充页表位0x100000*/
	uint32_t pagedir_phy_addr = 0x100000;
	if(pthread->pgdir != NULL){
		//用户进程有自己的页目录表
		pagedir_phy_addr = addr_v2p((uint32_t)pthread->pgdir);
	}
	/*更新页目录寄存器cr3，使新的页表生效*/
	asm volatile("mov %0, %%cr3" :: "r"(pagedir_phy_addr) : "memory");
}

/*激活线程或进程的页表，更新tss中的esp0为进程的特权级0的栈*/
void process_activate(struct task_struct *pthread){
	ASSERT(pthread != NULL);
	/*激活该进程或线程的页表*/
	page_dir_activate(pthread);

	/*内核线程特权级本身为0，处理器进入中断时不会从tss中获取0特权级栈的地址，故不需要更新esp0*/
	if(pthread->pgdir){
		/*更新该进程的esp0，用于进程被中断时保存上下文*/
		update_tss_esp(pthread);
	}
}

/*创建页目录表，将当前页表的表示内核空间的pde复制，成功则返回页目录的虚拟地址，否则返回-1*/
uint32_t *create_page_dir(void){
	/*用户进程的页表不能让用户直接访问到，所以在内核空间直接申请*/
	uint32_t *page_dir_vaddr = get_kernel_pages(1);
	/*
	put_str("---------------yyyyyyyyyyyyyyyyy      ");
        put_int(page_dir_vaddr);
        put_str("\n");*/

	if(page_dir_vaddr == NULL){
		console_put_str("create_page_dir: get_kernel_page failed!");
		return NULL;
	}
	/*************************1 复制页表*******************************/
	memcpy((uint32_t *)((uint32_t)page_dir_vaddr + 0x300 * 4), \
			(uint32_t *)(0xfffff000 + 0x300 *4), 1024);


	/************************2 更新页目录地址*************************/
	uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t) page_dir_vaddr);
	page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;
	return page_dir_vaddr;
}


/*创建用户进程虚拟地址位图*/
void create_user_vaddr_bitmap(struct task_struct *user_prog){
	user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
	uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START)/PG_SIZE  /8 ,PG_SIZE);
	user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
	user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) /PG_SIZE / 8;
	bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}


/*创建用户进程*/
void process_execute(void* filename, char *name){

	struct task_struct *thread = get_kernel_pages(1);
	init_thread(thread, name, default_prio);
	create_user_vaddr_bitmap(thread);
	thread_create(thread, start_process, filename);
	thread->pgdir = create_page_dir();

	enum intr_status old_status = intr_disable();
	ASSERT(elem_find(&thread_ready_list, &thread->general_tag) == 0);
	list_append(&thread_ready_list, &thread->general_tag);

	ASSERT(elem_find(&thread_all_list, &thread->all_list_tag) == 0);
	list_append(&thread_all_list, &thread->all_list_tag);
	intr_set_status(old_status);

}
