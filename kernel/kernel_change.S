[bits 32]

%define ERROR_CODE nop

%define ZERO push 0

extern put_str;声明外部函数
extern put_int;声明外部函数
extern idt_table ;idt_table是存储中断处理程序的数组

section .data
intr_str: db "interrupt occur!",0xa,0
intr_str1: db "nononono!",0xa,0
global intr_entry_table

intr_entry_table:
%macro VECTOR 2
section .text
intr%1entry:
	%2
	;------------保护上下文环境-------------------
	push ds
	push es
	push fs
	push gs
	;PUSHAD指令压入32位寄存器，其入栈顺序是EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
	pushad

	;如果是从片上进入的中断
	;除了往从片上发送EOI外，还要往主片上发送EOI
	mov al,0x20
	out 0x20,al
	out 0xa0,al
	

	push %1  ;将中断号压入栈中
	call [idt_table + %1 * 4]
	jmp intr_exit
section .data
	dd intr%1entry
%endmacro

section .text
global intr_exit
intr_exit:
	;------------恢复上下文环境---------------------
	add esp,4
	popad
	pop gs
	pop fs
	pop es
	pop ds
	add esp,4  ;跳过error_code
	iretd

VECTOR 0X00,ZERO
VECTOR 0X01,ZERO
VECTOR 0X02,ZERO
VECTOR 0X03,ZERO
VECTOR 0X04,ZERO
VECTOR 0X05,ZERO
VECTOR 0X06,ZERO
VECTOR 0X07,ZERO
VECTOR 0X08,ERROR_CODE
VECTOR 0X09,ZERO
VECTOR 0X0A,ERROR_CODE
VECTOR 0X0B,ERROR_CODE
VECTOR 0X0C,ZERO
VECTOR 0X0D,ERROR_CODE
VECTOR 0X0E,ERROR_CODE
VECTOR 0X0F,ZERO
VECTOR 0X10,ZERO
VECTOR 0X11,ERROR_CODE
VECTOR 0X12,ZERO
VECTOR 0X13,ZERO
VECTOR 0X14,ZERO
VECTOR 0X15,ZERO
VECTOR 0X16,ZERO
VECTOR 0X17,ZERO
VECTOR 0X18,ERROR_CODE
VECTOR 0X19,ZERO
VECTOR 0X1A,ERROR_CODE
VECTOR 0X1B,ERROR_CODE
VECTOR 0X1C,ZERO
VECTOR 0X1D,ERROR_CODE
VECTOR 0X1E,ERROR_CODE
VECTOR 0X1F,ZERO
VECTOR 0X20,ZERO ;时钟中断对应的入口
VECTOR 0X21,ZERO ;键盘对应的入口
VECTOR 0x22,ZERO ;级联用的
VECTOR 0x23,ZERO ;串口2对应的入口
VECTOR 0X24,ZERO ;串口1对应的入口
VECTOR 0X25,ZERO ;并口2对应的入口
VECTOR 0X26,ZERO ;软盘对应的入口
VECTOR 0x27,ZERO ;并口1对应的入口
VECTOR 0x28,ZERO ;实时时钟对应的入口
VECTOR 0x29,ZERO ;重定向
VECTOR 0x2a,ZERO ;保留
VECTOR 0x2b,ZERO ;保留
VECTOR 0x2c,ZERO ;ps/2鼠标
VECTOR 0X2d,ZERO ;fpu浮点单元异常
VECTOR 0x2e,ZERO ;硬盘
VECTOR 0x2f,ZERO ;保留

