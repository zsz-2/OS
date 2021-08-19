%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR

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

;用来记录检测出内存的大小
total_mem_bytes: dd 0

ards_buf: times 244 db 0
ards_nr: dw 0

loader_start:
	xor ebx,ebx
	mov edx,0x534d4150
	mov di,ards_buf
.e820_mem_loop:
	mov eax,0x0000e820
	mov ecx,20
	int 0x15
	jc .e820_failed_so_try_e801
	add di,cx
	inc word [ards_nr]
	cmp ebx,0
	jnz .e820_mem_loop

	;------------在所有的ards结构中，找出内存的容量----------------
	mov cx,[ards_nr]
	mov ebx,ards_buf
	xor edx,edx
.find_max_mem_area:
	mov eax,[ebx]
	add eax,[ebx + 8]
	add ebx,20
	cmp eax,edx
	jl .next_ards
	mov edx,eax
.next_ards:
	loop .find_max_mem_area
	jmp .mem_get_ok


	
;-------------------------------使用功能号0xe801获取内存大小-------------
	
.e820_failed_so_try_e801:
	mov ax,0xe801
	int 0x15
	jc .e810_failed_so_try_88

	;----------先计算低15MB的内存------------------
	mov cx,0x400
	mul cx
	shl edx,16
	and eax,0x0000FFFF
	or edx,eax
	add edx,0x100000
	mov esi,edx
	;---------将16MB以上的内存转换位byte为单位------
	xor eax,eax
	mov ax,bx
	mov ecx,0x10000
	mul ecx
	add esi,eax
	
	;--------因为内存在4GB以内，所以edx肯定为0--------
	mov edx,esi
	jmp .mem_get_ok
	
;---------------------------------使用功能号0x88获取内存大小，只能获取64MB之内的-------------
.e810_failed_so_try_88:
	xor eax,eax
	mov ah,0x88
	int 0x15
	jc .error_halt
	and eax,0x0000FFFF
	mov cx,0x400
	mul cx
	shl edx,16
	or edx,eax
	add edx,0x100000

.mem_get_ok:
	mov [total_mem_bytes],edx

	;-----------------进入实模式---------------------------------
	in al,0x92
	or al,0000_0010b
	out 0x92,al
	

	lgdt[gdt_ptr]

	mov eax,cr0
	or eax,0x00000001
	mov cr0,eax

	jmp SECTOR_CODE:.p_mode
[bits 32]
.p_mode:

	mov ax,SECTOR_DATA
	mov ds,ax
	mov es,ax
	mov fs,ax
	mov ss,ax
	mov ax,SECTOR_VIDEO
	mov gs,ax

	mov esp,LOADER_STACK_TOP

	;------------------加载内核-----------------------------------
	mov eax,KERNEL_START_SECTOR
	mov ebx,KERNEL_BIN_BASE_ADDR
	;从磁盘读出后，写到ebx指定的地址
	mov ecx,200
	call rd_disk_m_32

	call setup_page
	

	;------------------为重新加载GDT表作准备——---------------------
	sgdt [gdt_ptr]
	mov ebx,[gdt_ptr+2]
	;-----------------更改视频段描述符----------------------------
	or dword [ebx + 0x18 + 4],0xc0000000
	add dword [gdt_ptr + 2],0xc0000000

	add esp,0xc0000000 ;将指针映射到内核地址

	;把页目录地址赋予cr3
	mov eax,PAGE_DIR_TABLE_POS
	mov cr3,eax

	;打开cr0的pg位
	mov eax,cr0
	or eax,0x80000000
	mov cr0,eax

	;-------------开启分页之后重新加载gdt--------------------------
	lgdt [gdt_ptr]
	mov byte [gs:160],'V'

	jmp SECTOR_CODE : .enter_kernel

.enter_kernel:
	call kernel_init
	mov esp,0xc009f000
	jmp KERNEL_ENTRY_POINT

	jmp $

.error_halt:
	hlt


;----------------------------创建页目录和页表------------------------------
setup_page:
;先把页目录占用的空间逐字节清0
	mov ecx,4096
	mov esi,0
.clear_page_dir:
	mov byte [PAGE_DIR_TABLE_POS + esi], 0
	inc esi
	loop .clear_page_dir
;开始创建页目录项
.create_pde:
	mov eax,PAGE_DIR_TABLE_POS
	add eax,0x1000
	mov ebx,eax
	or eax, PG_US_U | PG_RW_W | PG_P
	mov dword [PAGE_DIR_TABLE_POS  + 0x0],eax
	mov dword [PAGE_DIR_TABLE_POS  + 0xc00],eax
	sub eax,0x1000
	mov dword [PAGE_DIR_TABLE_POS + 4092],eax

;开始创建页表项
	mov ecx,256
	mov esi,0
	mov edx,PG_US_U | PG_RW_W | PG_P
.create_pte:
	mov [ebx + esi * 4],edx
	add edx,4096
	inc esi
	loop .create_pte
	mov eax,PAGE_DIR_TABLE_POS
	add eax,0x2000
	or eax,PG_US_U | PG_RW_W | PG_P
	mov ebx,PAGE_DIR_TABLE_POS
	mov ecx,254
	mov esi,769
.create_kernel_pde:
	mov [ebx + esi * 4],eax
	inc esi
	add eax,0x1000
	loop .create_kernel_pde
	ret

;---------------------从磁盘读取内核文件--------------------------------------
	;功能：读取硬盘n个扇区
	;eax=LBA扇区号
	;ebx=将数据写入的内存地址
	;ecx=读入的扇区数
rd_disk_m_32:
	mov esi,eax
	mov di,cx
;读写硬盘
;第一步：设置读取的扇区数
	mov dx,0x1f2
	mov al,cl
	out dx,al

	mov eax,esi   ;恢复ax
;第二步：将LBA地址存入0x1f3~0x1f6
	mov dx,0x1f3
	out dx,al

	mov cl,8
	shr eax,cl
	mov dx,0x1f4
	out dx,al

	shr eax,cl
	mov dx,0x1f5
	out dx,al

	shr eax,cl
	and al,0x0f
	or al,0xe0
	mov dx,0x1f6
	out dx,al

;第三步：向0x1f7端口写入读命令，0x20
	mov dx,0x1f7
	mov al,0x20
	out dx,al
	
;第四步：检测硬盘状态
.not_ready:
	nop
	in al,dx
	and al,0x88
	cmp al,0x08
	jnz .not_ready


;第五步：从0x1f0中读取数据
	mov ax,di
	;di表示要读取的扇区数，一个扇区有512个字节，每次读入一个字，需要di * 512 /2
	mov dx,256
	mul dx
	mov cx,ax
	mov dx,0x1f0
.go_on_read:
	in ax,dx
	mov [ebx],ax
	add ebx,2
	loop .go_on_read
	ret

;-------------------------内核初始化代码-----------------------
kernel_init:
	xor eax,eax
	xor ebx,ebx
	xor ecx,ecx
	xor edx,edx

	mov dx,[KERNEL_BIN_BASE_ADDR + 42]
	;偏移文件42字节是e_phentsize,表示program header的大小
	mov ebx,[KERNEL_BIN_BASE_ADDR + 28]
	;偏移文件28字节处是program header的在文件中的偏移量
	add ebx,KERNEL_BIN_BASE_ADDR
	mov cx,[KERNEL_BIN_BASE_ADDR + 44]
	;程序头表的数目

.each_segment:
	cmp byte [ebx + 0],PT_NULL
	je .PT_NULL
	
	;调用mem_cpy(dst,src,size)函数
	push dword [ebx + 16]
	;源地址
	mov eax,[ebx + 4]
	add eax,KERNEL_BIN_BASE_ADDR
	push eax
	;目的地址
	push dword [ebx + 8]

	call mem_cpy
	add esp,12
.PT_NULL:
	add ebx,edx
	loop .each_segment
	ret

;------------------------逐字节拷贝mem_cpy(dst,src,size)-------------------
mem_cpy:
	cld
	push ebp
	mov ebp,esp
	push ecx
	
	mov edi,[ebp + 8]
	mov esi,[ebp + 12]
	mov ecx,[ebp + 16]
	
	rep movsb
	pop ecx
	pop ebp	
	ret


	

	
