TI_GDT equ 000b
RPL0 equ 00b
SELECTOR_VIDEO equ (0x0003 << 3 ) + TI_GDT  + RPL0

;存储转换后的字符
section .data
put_int_buffer: dq 0

[bits 32]
;--------------------put_int--------------------------
;用于整数打印
;输入：栈中参数位待打印数字
;输出：16进制数字
global put_int
put_int:
	pushad
	mov ebp,esp
	mov eax,[ebp + 36]
	mov edx,eax
	mov edi,7
	mov ecx,8
	mov ebx,put_int_buffer
.based4bits:
	and edx,0x0000000F
	cmp edx,9
	jg .isA2F
	add edx,'0'
	jmp .store
.isA2F:
	sub edx,10
	add edx,'A'
.store:
	mov [ebx + edi],dl
	dec edi
	shr eax,4
	mov edx,eax
	loop .based4bits

;去掉0
.ready_to_print:
	inc edi
.skip_prefix_0:
	cmp edi,8
	je .full0
.go_on_skip:
	mov cl,[ebx + edi]
	inc edi
	cmp cl,'0'
	je .skip_prefix_0
	dec edi
	jmp .put_each_num

.full0:
	mov cl,'0'
.put_each_num:
	push ecx
	call put_char
	add esp,4
	inc edi
	cmp edi,8
	jge .over
	mov cl,[ebx + edi]
	jmp .put_each_num

.over:
	popad;
	ret
	

	


;----------------------put_str-------------------------
global put_str
put_str:
	push ebx
	push ecx
	xor ecx,ecx
	;待打印字符串的地址
	mov ebx,[esp + 12]
	
.goon:
	mov cl,[ebx]
	cmp cl,0
	jz .str_over
	push ecx
	call put_char
	add esp,4
	inc ebx
	jmp .goon
.str_over:
	pop ecx
	pop ebx
	ret
	



;-----------------------put char------------------------
;功能描述： 将栈中的一个字符打印出来
;-------------------------------------------------------
global put_char
put_char:
	pushad    ;备份32位寄存器环境
	
	;切换段选择子，确保访问到显存
	mov ax,SELECTOR_VIDEO
	mov gs,ax


;------------------获取当前光标位置---------------------

	;----------获得光标的高8位----------------------
	mov dx,0x3d4   ;索引寄存器
	mov al,0x0e
	out dx,al
	mov dx,0x3d5  ;data register
	in al,dx
	mov ah,al

	;----------获得光标的低8位----------------------
	mov dx,0x3d4
	mov al,0x0f
	out dx,al
	mov dx,0x3d5
	in al,dx

	mov bx,ax
	mov ecx,[esp + 36]

	cmp cl,0xd  ;-------判断是否是回车符-----------
	jz is_carriage_return
	cmp cl,0xa  ;-------判断是否是换行符-----------
	jz is_line_feed
	
	cmp cl,0x8
	jz  is_backspace
	jmp put_other

is_backspace:
	;------------------ 删除字符的操作 ------------------
	;------------------ 将字符变为空--------------------
	dec bx
	shl bx,1
	mov byte [gs:bx],0x20
	inc bx
	mov byte [gs:bx],0x07
	shr bx,1
	jmp set_cursor

put_other:
	shl bx,1
	mov byte [gs:bx],cl
	inc bx
	mov byte [gs:bx],0x07
	shr bx,1
	inc bx
	cmp bx,2000
	jl set_cursor

is_line_feed:
is_carriage_return: ;---------回车符-----------
	xor dx,dx
	mov ax,bx
	mov si,80
	div si
	sub bx,dx

is_carriage_return_end:
	add bx,80
	cmp bx,2000
is_line_feed_end:
	jl set_cursor
	
roll_screen:
	cld
	;一共需要搬运1920个字符，即3840个字节，一次搬运4字节，搬运960次
	mov ecx,960
	mov esi,0xc00b80a0
	mov edi,0xc00b8000
	rep movsd
	
	
;---------------清空最后一行——-------------------------
	mov ebx,3840
	mov ecx,80
cls:
	mov word [gs:ebx],0x0720
	add ebx,2
	loop cls
	mov bx,1920
set_cursor:
	mov dx,0x03d4
	mov al,0x0e
	out dx,al
	mov dx,0x03d5
	mov al,bh
	out dx,al


	mov dx,0x03d4
	mov al,0x0f
	out dx,al
	mov dx,0x03d5
	mov al,bl
	out dx,al

put_char_done:
	popad
	ret
