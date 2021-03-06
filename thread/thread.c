#include "thread.h"
#include "userprog.h"
#include "sync.h"
#include "debug.h"
#include "interrupt.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "list.h"

#define PG_SIZE 4096
#define NULL 0

struct task_struct *main_thread; //主线程PCB
struct task_struct *idle_thread; //idle线程
static struct list_elem* thread_tag; //用于保存队列中的线程结点

extern void switch_to(struct task_struct *curr, struct task_struct *next);

struct lock pid_lock;

/*分配pid*/
static pid_t allocate_pid(void){
	static pid_t next_pid  = 0;
	lock_acquire(&pid_lock);
	next_pid++;
	lock_release(&pid_lock);
	return next_pid;
}

pid_t fork_pid(void){
	return allocate_pid();
}

/*获取PCB指针*/
struct task_struct *running_thread(){
	uint32_t esp;
	asm ("mov %%esp, %0" : "=g"(esp)::"memory");
	return (struct task_struct *)(esp & 0xfffff000);
}


/*由kernel_thread去执行function(arg)*/
static void kernel_thread(thread_func *function, void *func_arg){
	intr_enable();
	function(func_arg);
}

/*初始化线程栈，将待执行函数和参数放在thread_stack中的相应位置*/
void thread_create(struct task_struct* pthread, thread_func *function, void *func_arg){
	/*先预留中断使用的栈空间*/
	pthread->self_kstack -= sizeof(struct intr_stack);

	/*再留出线程空间*/
	pthread->self_kstack -= sizeof(struct thread_stack);
	struct thread_stack *kthread_stack = (struct thread_stack *)pthread->self_kstack;
	kthread_stack->eip = kernel_thread;
	kthread_stack->function = function;
	kthread_stack->func_arg = func_arg;
	kthread_stack->ebp = kthread_stack->ebx = \
		kthread_stack->esi = kthread_stack->edi  = 0;
}

/*初始化线程基本信息*/
void init_thread(struct task_struct *pthread, char *name, int prio){
	memset(pthread, 0, sizeof(*pthread));
	pthread->pid = allocate_pid();
	strcpy(pthread->name, name);
	if(pthread == main_thread) pthread->status = TASK_RUNNING;
	else pthread->status = TASK_READY;
	pthread->priority = prio;
	pthread->ticks = prio;
	pthread->elapsed_ticks = 0;
	pthread->pgdir = NULL;
	/*self_kstack是线程自己在内核态下使用的栈顶地址*/
	pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
	pthread->cwd_inode_nr = 0; //以根目录作为默认工作路径
	pthread->parent_pid = -1; //使任务的父进程默认为-1
	pthread->stack_magic = 0x19990512;
	/*预留标准输出输出*/
	pthread->fd_table[0] = 0;
	pthread->fd_table[1] = 1;
	pthread->fd_table[2] = 2;
	/*其余全置为-1*/
	uint8_t fd_idx = 3;
	while(fd_idx < MAX_FILES_OPEN_PER_PROC){
		pthread->fd_table[fd_idx] = -1;
		++fd_idx;
	}

}

/*创建一优先级位prio的线程，名为name,线程执行的函数是function(func_arg)*/
struct task_struct *thread_start(char *name, int prio, thread_func function, void *func_arg){
	/*pcb都位于内核空间，包括用户进程的pcb也在内核空间*/
	struct task_struct *thread = get_kernel_pages(1);
	init_thread(thread, name, prio);
	thread_create(thread, function, func_arg);

	ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
	list_append(&thread_ready_list, &thread->general_tag);

	ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
	list_append(&thread_all_list, &thread->all_list_tag);
	
	/*
	put_str("all_len: ");
	put_int(list_len(&thread_all_list));
	puot_str("\n");
	*/
	
	return thread;
}

/*将kernel中的main函数完善为主线程*/
static void make_main_thread(void){
	main_thread = running_thread();
	init_thread(main_thread, "main", 100);

	/*main是当前线程，当前线程不在thread_ready_list*/
	ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
	list_append(&thread_all_list, &main_thread->all_list_tag);
}

/*实现任务调度*/
void schedule(){
	ASSERT(intr_get_status() == INTR_OFF);
	struct task_struct *cur= running_thread();
	if(cur->status == TASK_RUNNING){
		//若线程只是CPU时间到了，将其加入就绪队列队尾
		ASSERT(elem_find(&thread_ready_list, &cur->general_tag) == 0);
		list_append(&thread_ready_list, &cur->general_tag);
		cur->ticks = cur->priority;
		cur->status = TASK_READY;
	}else{
		/*若此线程需要某事发生之后才能继续上CPU运行，不需要加入队列中，因为当前线程不在就绪队列中*/
	}
	/*
	put_str("asssssssssssss:         ");
	put_int(list_len(&thread_all_list));
	put_str("\n");*/
	if(list_empty(&thread_ready_list) == 1){
		thread_unblock(idle_thread);
	}
	thread_tag = NULL;
	thread_tag = list_pop(&thread_ready_list);
	struct task_struct *next = elem2entry(struct task_struct, general_tag, thread_tag);
	next->status = TASK_RUNNING;

	/*激活任务页表*/
	process_activate(next);
	//put_str("schedule: \n");

	switch_to(cur, next);
}


/*当前线程将自己堵塞*/
void thread_block(enum task_status stat){
	/*stat取值为TASK_BLOCKED, TASK_WAITTING, TASK_HANGING,这三种状态才不会被调度*/
	ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITTING) || (stat == TASK_HANGING)));
	enum intr_status  old_status  = intr_disable();
	struct task_struct *cur_thread = running_thread();
	cur_thread->status = stat;
	schedule();  //将当前线程换下处理器
	intr_set_status(old_status);
}

/*将线程pthread阻塞*/
void thread_unblock(struct task_struct *pthread){
	enum intr_status old_status = intr_disable();
	ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITTING || pthread->status == TASK_HANGING)));
	if(pthread->status != TASK_READY){
		ASSERT(elem_find(&thread_ready_list, &pthread->general_tag) == 0);
		list_push(&thread_ready_list, &pthread->general_tag);
		pthread->status = TASK_READY;
	}
	intr_set_status(old_status);
}

/*thread_yield是主动把CPU使用权让出，让其他线程运行*/
void thread_yield(void){
	struct task_struct *cur = running_thread();
	enum intr_status old_status = intr_disable();
	ASSERT(elem_find(&thread_ready_list, &cur->general_tag) == 0);
	list_append(&thread_ready_list, &cur->general_tag);
	cur->status = TASK_READY;
	schedule();
	intr_set_status(old_status);
}

/*系统空闲时运行的线程*/
static void idle(void *arg){
	while(1){
		thread_block(TASK_BLOCKED);
		asm volatile("sti; hlt":::"memory");
	}
}

extern void init(void);
void thread_init(){
	put_str("thread_init  start\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	lock_init(&pid_lock);
	process_execute(init, "init");
	make_main_thread();
	idle_thread = thread_start("idle", 10, idle, NULL);
	put_str("thread_init done\n");
}

