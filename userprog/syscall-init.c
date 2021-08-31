#include "thread.h"
#include "stdint.h"
#include "global.h"
#include "interrupt.h"
#include "syscall.h"
#include "io.h"

#define syscall_nr 32
typedef void* syscall;

syscall syscall_table[syscall_nr];

uint32_t sys_getpid(void){
	return running_thread()->pid;
}

/*初始化系统调用*/
void syscall_init(void){
	put_str("syscall_init start\n");
	syscall_table[SYS_GETPID] = sys_getpid;
	put_str("syscall init down\n");
}
