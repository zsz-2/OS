#include "thread.h""
#include "fs.h"
#include "memory.h"
#include "string.h"
#include "stdint.h"
#include "global.h"
#include "interrupt.h"
#include "syscall.h"
#include "io.h"

#define syscall_nr 32
typedef void* syscall;

extern pid_t sys_fork(void);
syscall syscall_table[syscall_nr];

uint32_t sys_getpid(void){
	return running_thread()->pid;
}

/*
uint32_t sys_write(char *str){
	console_put_str(str);
	return strlen(str);
}
*/

/*初始化系统调用*/
void syscall_init(void){
	put_str("syscall_init start\n");
	syscall_table[SYS_GETPID] = sys_getpid;
	syscall_table[SYS_WRITE] = sys_write;
	syscall_table[SYS_MALLOC] = sys_malloc;
	syscall_table[SYS_FREE] = sys_free;
	syscall_table[SYS_FORK] = sys_fork;
	put_str("syscall init down\n");
}
