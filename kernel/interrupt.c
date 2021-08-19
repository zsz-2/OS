#include "interrupt.h"
#include "print.h"
#include "stdint.h"
#include "global.h"
#include "io.h"

#define IDT_DESC_CNT 0x21


#define EFLAGS_IF 0x00000200
#define GET_EFLAGS (EFLAG_VAR) asm volatile ("pushfl; popl %0" : "=g"(EFLAG_VAR)::"memory")

#define PIC_M_CTRL 0x20   //主片的控制端口是0x20
#define PIC_M_DATA 0x21   //主片的数据端口是0x21
#define PIC_S_CTRL 0xa0   //从片的控制端口是0xa0
#define PIC_S_DATA 0xa1   //从片的数据端口是0xa1

/*中断门描述符结构体*/
struct gate_desc{
	uint16_t func_offset_low_word;
	uint16_t selector;
	uint8_t dcount;
	uint8_t attribute;
	uint16_t func_offset_high_word;
};

//静态函数声明
static void make_idt_desc(struct gate_desc *p_gdesc, uint8_t attr,intr_handler function);
/*开中断并返回中断之前的状态*/
enum intr_status intr_enable();
/*关中断并返回中断之前的状态*/
enum intr_status intr_disable();
/*获取当前的中断状态*/
enum intr_status intr_get_status();
/*将中断设置为status*/
enum intr_status intr_set_status(enum intr_status status);

static struct gate_desc idt[IDT_DESC_CNT];
extern intr_handler intr_entry_table[IDT_DESC_CNT];


/*创建中断们描述符*/
static void make_idt_desc(struct gate_desc *p_gdesc, uint8_t attr, intr_handler function){
	p_gdesc->func_offset_low_word = (uint32_t)function &0x0000FFFF;
	p_gdesc->dcount = 0;
	p_gdesc->attribute = attr;
	p_gdesc->selector = SELECTOR_K_CODE;
	p_gdesc->func_offset_high_word = ((uint32_t)function &0xFFFF0000)>>16;
}

/*初始化中断描述符表*/
static void idt_desc_init(void){
	int i;
	for(i = 0; i < IDT_DESC_CNT;++i){
		make_idt_desc(&idt[i],IDT_DESC_ATTR_DPL0,intr_entry_table[i]);
	}
	put_str("   idt_desc_init done\n");
}

static void pic_init(void){
	//初始化主片
	outb(PIC_M_CTRL,0x11); //ICW1,边沿触发，级联8259，需要ICW4
	outb(PIC_M_DATA,0x20); //ICW2:起始中断向量号为0x20
	outb(PIC_M_DATA,0x04); //ICW3:IR2接从片
	outb(PIC_M_DATA,0x01); //ICW4:非自动EOI,8086模式
	//初始化从片
	outb(PIC_S_CTRL,0x11); //ICW1,边沿触发，级联8259,需要ICW4
	outb(PIC_S_DATA,0x28); //起始中断向量号为0x28
	outb(PIC_S_DATA,0x02); //设置从片连接到主片的IR2引脚
	outb(PIC_S_DATA,0x01); //ICW4：非自动EOI，8086模式

	//只打开时钟中断
	outb(PIC_M_DATA,0xfe);
	outb(PIC_S_DATA,0xff);
	put_str("  pic_init done\n");
}

/*关于中断的初始化工作*/
void idt_init(){
	put_str("idt_init start   \n");
	idt_desc_init();
	pic_init();
	/*加载idt*/
	//lidt会取出64位的前48位，指针只能转换为相同大小的整型
	uint64_t idt_operand = (sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16);
	asm volatile  ("lidt %0;"::"m"(idt_operand));

	asm volatile ("sti;");
	while(1);
	put_str("idt_init  done\n");
}


/*开中断并返回开中断前的中断*/
enum intr_status intr_enable(){
	enum intr_status old_status;
	if(INTR_ON == intr_get_status()){
		old_status = INTR_ON;
		return old_status;
	}
	else{
		old_status = INTR_OFF;
		asm volatile ("sti":::"memory");
		return old_status;
	}
}

/*关中断并返回开中断前的中断*/
enum intr_status intr_disable(){
	enum intr_status old_status = INTR_OFF;
	if(INTR_OFF == intr_get_status()){
		return old_status;
	}
	else{
		old_status = INTR_ON;
		asm volatile ("cli" : : : "memory");
		return old_status;
	}

}

/*设置当前status状态*/
enum intr_status intr_set_status(enum intr_status status){
	return status & INTR_ON ? intr_enable() : intr_disable();
}

/*获取当前中断状态*/
enum intr_status intr_get_status(){
	uint32_t eflags  = 0;
	GET_EFLAGS(eflags);
	return (eflags & EFLAGS_IF) ? INTR_ON : INTR_OFF;
}

