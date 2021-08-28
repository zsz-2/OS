global set_coordinate
set_coordinate:
	pushad
	mov ebp,esp
	mov ebx,[ebp + 36]

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
	
	popad
	ret
