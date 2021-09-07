#include "interrupt.h"
#include "print.h"
#include "stdint.h"
#include "global.h"
#include "io.h"

#define IDT_DESC_CNT 0x81

extern uint32_t syscall_handler(void);


#define EFLAGS_IF 0x00000200
#define GET_EFLAGS(EFLAG_VAR) asm volatile ("pushfl; popl %0" : "=g"(EFLAG_VAR)::"memory")


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
static struct gate_desc idt[IDT_DESC_CNT];

//用于保存异常的名字
char *intr_name[IDT_DESC_CNT];
intr_handler idt_table[IDT_DESC_CNT];
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
	int i, lastindex = IDT_DESC_CNT  - 1;
	for(i = 0; i < IDT_DESC_CNT;++i){
		make_idt_desc(&idt[i],IDT_DESC_ATTR_DPL0,intr_entry_table[i]);
	}
	make_idt_desc(&idt[lastindex], IDT_DESC_ATTR_DPL3, syscall_handler);
	put_str("   idt_desc_init done\n");
}

//8259A芯片设置
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
	outb(PIC_M_DATA,0xf8);
	outb(PIC_S_DATA,0xbf);
	put_str("  pic_init done\n");
}

//通用的中断处理函数，在异常发生时使用
static void general_intr_handler(uint8_t vec_nr){
	//IRQ7和IRQ15会产生伪中断，并不是真正的中断，而是硬件设备出现问题，无法通过IMR寄存器屏蔽，最好直接忽视
	if(vec_nr == 0x27 || vec_nr == 0x2f){
		return;
	}

	/*将光标设置为0，在屏幕左上角清出一片信息*/
	set_coordinate(0);
	int current_pos = 0;
	while(current_pos < 320){
		put_char(' ');
		++current_pos;
	}
	set_coordinate(0);
	put_str("!!!!!!!!!!    excetion message begin   !!!!!!!!!!!\n");
	put_str(intr_name[vec_nr]);
	
	if(vec_nr == 14){
		int page_fault_vaddr = 0;
		asm volatile ("movl %%cr2,%0 ":"=r"(page_fault_vaddr));
		put_str("\npage fault addr is: ");
		put_int(page_fault_vaddr);
	}
	put_str("\n!!!!!!!!!!!!!      excetion message end    !!!!!!!!!!!!\n");
	while(1); //此时中断已经关闭
}


/*完成一般中断处理函数注册以及异常名称注册*/
static void exception_init(void){
	int i;
	for(i = 0; i < IDT_DESC_CNT;++i){
		idt_table[i] = general_intr_handler;
		intr_name[i] = "unknown";
	}

	intr_name[0] = "#DE Divide Error";
	intr_name[1] = "#DB Debug Exception";
	intr_name[2] = "NMI Interrupt";
	intr_name[3] = "#BP Breakpoint Exception";
	intr_name[4] = "#OF Overflow Exception";
	intr_name[5] = "#BR BOUND Range Exceeded Exception";
	intr_name[6] = "#UD Invalid Opcode Exception";
	intr_name[7] = "#NM Device Not Available Exception";
	intr_name[8] = "#DF Double Fault Exception";
	intr_name[9] = "Coprocessor Segment Overrun";
	intr_name[10] = "#TS invalid TSS Exception";
	intr_name[11] = "#NP Segment Not Present";
	intr_name[12] = "#SS Stack Fault Exception";
	intr_name[13] = "#GP General Protecetion Exception";
	intr_name[14] = "#PF Page-Fault Exception";
	//intr_name[15] = ""; 15号为intel保留项，未被使用
	intr_name[16] = "#MF x87 FPU Floating-Point Error";
	intr_name[17] = "#AC Alignment Check Exception";
	intr_name[18] = "#MC Machine-Check Exception";
	intr_name[19] = "#XF SIMD Floating-Point Exception";
}

/*在中断处理程序数组第vector_no注册安装中断处理程序function*/
void register_handler(uint8_t vector_no, intr_handler function){
	idt_table[vector_no] = function;
}


/*关于中断的初始化工作*/
void idt_init(){
	put_str("idt_init start   \n");
	idt_desc_init();
	exception_init();
	pic_init();
	/*加载idt*/
	//lidt会取出64位的前48位，指针只能转换为相同大小的整型
	uint64_t idt_operand = (sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16);
	asm volatile  ("lidt %0;"::"m"(idt_operand));
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

