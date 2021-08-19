#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"
typedef void * intr_handler;
void idt_init(void);

/*定义中断的两种状态，intr_off代表中断关闭，intr_on代表中断打开*/
enum intr_status{
	INTR_OFF,
	INTR_ON
};

/*开中断并返回中断之前的状态*/
enum intr_status intr_enable();
/*关中断并返回中断之前的状态*/
enum intr_status intr_disable();
/*获取当前的中断状态*/
enum intr_status intr_get_status();
/*将中断设置为status*/
enum intr_status intr_set_status(enum intr_status status);
#endif
