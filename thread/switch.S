[bits 32]
section .text
global switch_to
switch_to:
	push esi
	push edx
	push ebx
	push ebp

	mov  eax, [esp + 20]
	mov [eax], esp ;保存栈顶的指针

	;----------------------恢复下一次线程的环境-----------------------------
	mov eax,[esp + 24]
	mov esp,[eax]

	pop ebp
	pop ebx
	pop edx
	pop esi
	;jmp $
	ret
	
