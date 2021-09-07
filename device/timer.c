#include "timer.h"
#include "interrupt.h"
#include "debug.h"
#include "thread.h"
#include "stdint.h"
#include "io.h"
#include "print.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY	1193180
#define COUNTER0_VALUE	INPUT_FREQUENCY	/ IRQ0_FREQUENCY
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER0_MODE 2
#define READ_WRITE_LATCH 3
#define PIC_CONTROL_PORT 0x43


/*把操作的计数器counter_no,读写属性,计数器模式写入控制寄存器中*/
static void frequency_set(uint8_t counter_port,\
		uint8_t counter_no,\
		uint8_t rwl,\
		uint8_t counter_mode,\
		uint16_t counter_value){
	outb(PIC_CONTROL_PORT, (uint8_t)((counter_no << 6) | (rwl << 4)  | (counter_mode << 1)) );
	//写入counter_value的低8位
	outb(counter_port, (uint8_t)counter_value);
	//写入counter_value的高8位
	outb(counter_port, (uint8_t)(counter_value>>8));
}


uint32_t ticks;  //ticks是内核中断开启以来总共的滴答数

/*时间中断处理函数*/
static void intr_timer_handler(void){

	struct task_struct *cur_thread = running_thread();
	ASSERT(cur_thread->stack_magic == 0x19990512);
	++cur_thread->elapsed_ticks;     //记录此线程占用的CPU时间
	++ticks; //内核第一次处理时间中断后开始至今的滴答数

	if(cur_thread->ticks == 0){
		schedule(); //调度新的线程
	}else{
		--cur_thread->ticks;
	}

}


void timer_init(){
	put_str("timer_init start\n");
	/*设置8253的定时周期，即发中断的周期*/
	frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER0_MODE, COUNTER0_VALUE);
	register_handler(0x20, intr_timer_handler);
	put_str("time init done\n");
}

/*以ticks为单位的sleep，任何时间形式的sleep会转换为ticks形式*/
static void ticks_to_sleep(uint32_t sleep_ticks){
	uint32_t start_tick = ticks;
	/*间隔的ticks不够便让出cpu*/
	while(ticks - start_tick < sleep_ticks){
		thread_yield();
	}
}


#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)

/*以毫秒单位的sleep， 1s = 1000ms*/
void mtime_sleep(uint32_t m_seconds){
	uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
	ASSERT(sleep_ticks > 0);
	ticks_to_sleep(sleep_ticks);
}
