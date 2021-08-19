#include "timer.h"
#include "stdint.h"
#include "io.h"
#include "print.h"

#define IRQ0_FREQUENCY 1
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

void timer_init(){
	put_str("timer_init start\n");
	/*设置8253的定时周期，即发中断的周期*/
	frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER0_MODE, COUNTER0_VALUE);
	put_str("time init done\n");
}
