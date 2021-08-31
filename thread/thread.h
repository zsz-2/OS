#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "thread.h"
#include "userprog.h"
#include "debug.h"
#include "interrupt.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "list.h"


#define PG_SIZE 4096

/*自定义通用函数类型，在很多线程函数中当作形参类型*/
typedef void thread_func(void *);


/*进程或线程的状态*/
enum task_status{
	TASK_RUNNING,
	TASK_READY,
	TASK_BLOCKED,
	TASK_WAITTING,
	TASK_HANGING,
	TASK_DIED
};

/******************中断栈intr_stack*****************
 * 此结构用于中断发生时保护程序的上下文环境
 * 进程或线程被外部中断或软中断打断时，会按此结构压入上下文
 * 寄存器，intr_exit的出栈操作是此结构的逆操作
 * 此栈在自己的内核栈中位置固定，所在页的最顶端
 * *************************************************/
struct intr_stack{
	uint32_t vec_no;
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t esp_dummy;   //esp会被popad忽略
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;

	/*以下由cpu从低特权级到高特权级压入*/
	uint32_t err_code;
	void (*eip) (void);
	uint32_t cs;
	uint32_t eflags;
	void *esp;
	uint32_t ss;
};

/*****************线程栈thread_stack*******************
 * 在线程首次运行的时候，线程栈用于存储创建线程所需的相关数据
 * 和线程有关的数据都应该在PCB中
 */
struct thread_stack{
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edi;
	uint32_t esi;

	void (*eip) (thread_func *func, void *func_arg);

	//以下3句仅供第一次被调度上CPU时使用
	void (*unused_retadder);  //占位,调用eip函数时，会esp+4寻找函数的参数


	thread_func* function; //由kernel_thread所调用的函数名
	void *func_arg; //由kernel_thread所调用的函数所需的参数
};

/*进程或线程的pcb，程序控制块*/
struct task_struct{
	uint32_t *self_kstack; //各内核线程都有自己的内核栈
	enum task_status status;
	uint8_t priority;      //线程优先级
	uint8_t ticks; //每次在处理器上执行的时间滴答数
	uint32_t elapsed_ticks;//从任务执行之后，占用了多少滴答数
	struct list_elem general_tag;  //general_tag的作用是在一般队列上的结点
	struct list_elem all_list_tag; //线程被加入全部线程队列时使用
	uint32_t *pgdir; //进程自己页表的虚拟地址
	char name[16];
	struct virtual_addr userprog_vaddr;  //用户进程的虚拟地址
	uint32_t stack_magic;
};


void thread_init(void);
struct task_struct *running_thread();
#endif
