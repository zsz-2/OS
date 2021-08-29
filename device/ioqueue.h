#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64
#define NULL 0
#define true 1
#define false 0

typedef int bool;
/*环形队列*/
struct ioqueue{
	//生产者消费者问题
	struct lock lock;

	/*生产者，缓冲区不满时就放数据，否则睡眠*/
	struct task_struct *producer;
	
	/*消费者*/
	struct task_struct *consumer;
	char buf[bufsize];   //缓冲区大小
	int32_t head;  //队首，数据往队首处写
	int32_t tail;  //队尾，数据从队尾 读出
};

void ioqueue_init(struct ioqueue *ioq);
bool ioq_full(struct ioqueue *ioq);
bool ioq_empty(struct ioqueue *ioq);
char ioq_getchar(struct ioqueue *ioq);
void ioq_putchar(struct ioqueue *ioq, char byte);
#endif
