%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR
jmp loader_start

;构建gdt及其内部的描述符
GDT_BASE: dd 0x00000000
	  dd 0x00000000

CODE_DESC dd 0x0000FFFF
	  dd DESC_CODE_HIGH_4

DATA_STACK_DESC:  dd 0x0000FFFF
	          dd DESC_DATA_HIGH_4

VIDEO_DESC: dd 0x80000007
	    dd  DESC_VIDEO_HIGH4	

GDT_SIZE equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1
times 60 dq 0

;--------------------------定义选择子——-------------------
SECTOR_CODE equ (0x0001 << 3) + TI_GDT + RPL0
SECTOR_DATA equ (0x0002 << 3) + TI_GDT + RPL0
SECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

;-------------------------定义gdt的内容-------------------
gdt_ptr dw GDT_LIMIT
	dd GDT_BASE

loadermsg db '2 loader in real'

loader_start:
	;jmp $
;---------------------------------------------------------
;INT 0x10  功能号：0x13  功能描述：打印字符串
;---------------------------------------------------------	
;输入
;AH 子功能号=13H
;BH 页码
;Bl 属性
;CX 字符串长度
;(DH, DL) = 坐标（行，列）
;es:bp 字符串地址
;AL=显示输出方式
;0--------------字符串中只含显示字符，显示属性在BL中，光标位置不变
;1--------------字符串中只含显示字符，显示属性在BL中，光标位置变
;2--------------字符串中含显示字符和显示属性，光标位置不变
;3--------------字符串中含显示字符和显示属性，显示属性在BL中，光标位置变
;无返回值
	mov sp,LOADER_BASE_ADDR
	mov bp,loadermsg
	mov cx,16
	mov ax,0x1301
	mov bx,0x001f
	mov dx,0x1800
	int 0x10
;----------------------准备进入保护模式---------------------
;1.打开A20
;2.加载gdt
;3.将cr0的pe位置1
	;--------------打开A20----------------
	in al,0x92
	or al,0000_0010B
	out 0x92,al
	
	;-------------加载GDT-----------------
	lgdt [gdt_ptr]

	;------------cr0第0位置1--------------
	mov eax,cr0
	or eax, 0x00000001
	mov cr0,eax
	
	jmp dword SECTOR_CODE : p_mode_start

[bits 32]
p_mode_start:
	mov ax,SECTOR_DATA
	mov ds,ax
	mov es,ax
	mov ss,ax
	mov esp,LOADER_STACK_TOP
	mov ax,SECTOR_VIDEO
	mov gs,ax
	
	mov byte [gs:160],'P'
	jmp $

