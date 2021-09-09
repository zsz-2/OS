#include "init.h"
#include "interrupt.h"
#include "print.h"
#include "interrupt.h"
#include "../device/timer.h"
#include "thread.h"
#include "memory.h"
#include "console.h"
#include "keyboard.h"
#include "syscall-init.h"
#include "ide.h"
#include "fs.h"
/*初始化所有模块*/
void init_all(){
	put_str("init_all\n");
	idt_init(); //初始化中断
	timer_init();
	mem_init();
	thread_init();
	console_init();
	keyboard_init();
	tss_init();
	syscall_init();
	enum intr_status old_status =  intr_enable();
	ide_init();
	intr_set_status(old_status);
	filesys_init2();
}


