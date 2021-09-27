#include "sync.h"
#include "debug.h"
#include "interrupt.h"
#include "list.h"
#include "thread.h"
#include "stdint.h"

#define NULL 0

/*初始化信号量*/
void sema_init(struct semaphore *psema, uint8_t value){
	psema->value = value;   //为信号量赋初值
	list_init(&psema->waiters);	//初始化信号量的等待队列
}

/*初始化plock*/
void lock_init(struct lock *plock){
	plock->holder = NULL;
	plock->holder_repeat_nr = 0;
	sema_init(&plock->semaphore, 1);
}

/*信号量down操作*/
void sema_down(struct semaphore *psema){
	/*关中断表示原子操作*/
	enum intr_status old_status = intr_disable();
	//若value等于0，表示被别人持有
	while(psema->value == 0){
		ASSERT(elem_find(&psema->waiters, (struct list_elem *)&(running_thread()->general_tag)) == 0);
		/*当前线程不应该在等待队列中*/
		/*将自己加入等待队列中*/
		list_append(&psema->waiters, &(running_thread()->general_tag));
		thread_block(TASK_BLOCKED);  //阻塞线程，直至被唤醒
	}
	--psema->value;
	ASSERT(psema->value == 0);
	/*恢复之前的中断状态*/
	intr_set_status(old_status);
}

/*信号量的up操作*/
void sema_up(struct semaphore *psema){
	/*关中断表示原子操作*/
	enum intr_status old_status = intr_disable();
	ASSERT(psema->value == 0);
	if(list_empty(&psema->waiters)  == 0){
		struct task_struct *thread_blocked = elem2entry(struct task_struct, general_tag, \
				list_pop(&psema->waiters));
		thread_unblock(thread_blocked);
	}
	++psema->value;
	ASSERT(psema->value == 1);
	/*恢复之前的中断状态*/
	intr_set_status(old_status);
}

/*获取锁plock*/
void lock_acquire(struct lock *plock){
	/*排除自己持有锁但还未释放的情况*/
	if(plock->holder != running_thread()){
		sema_down(&plock->semaphore);
		plock->holder = running_thread();
		/*
		put_str("xixixiixi:  ");
		put_int(plock->holder_repeat_nr);
		put_str("\n");*/
		ASSERT(plock->holder_repeat_nr == 0);
		plock->holder_repeat_nr = 1;
	}else{
		++plock->holder_repeat_nr;
	}
}


/*释放锁*/
void lock_release(struct lock *plock){
	ASSERT(plock->holder == running_thread());
	if(plock->holder_repeat_nr > 1){
		--plock->holder_repeat_nr;
		return;
	}
	/*
	put_str("hahaha:  ");
	put_int(plock->holder_repeat_nr);
	put_str("\n"); */
	ASSERT(plock->holder_repeat_nr == 1);
	plock->holder = NULL;
	plock->holder_repeat_nr = 0;
	sema_up(&plock->semaphore);
}

