#include "syscall.h"
#include "thread.h"

/*无参数的系统调用*/
#define _syscall0(NUMBER) ({\
	int retval;\
	asm volatile( \
	"int $0x80" \
	:"=a"(retval)\
	:"a"(NUMBER) \
	:"memory");\
	retval; \
})

/*1参数的系统调用*/
#define _syscall1(NUMBER, ARG1) ({\
	int retval;\
	asm volatile( \
	"int $0x80" \
	:"=a"(retval)\
	:"0"(NUMBER),"b"(ARG1) \
	:"memory");\
	retval;\
})


/*2参数的系统调用*/
#define _syscall2(NUMBER, ARG1, ARG2) ({\
	int retval;\
	asm volatile( \
	"int $0x80" \
	:"=a"(retval)\
	:"0"(NUMBER),"b"(ARG1), "c"(ARG2) \
	:"memory");\
	retval;\
})

/*3参数的系统调用*/
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({\
	int retval;\
	asm volatile( \
	"int $0x80" \
	:"=a"(retval)\
	:"0"(NUMBER),"b"(ARG1), "c"(ARG2), "d"(ARG3) \
	:"memory");\
	retval;\
})





/*返回当前任务的pid*/
uint32_t getpid(){
	//while(1);
	return _syscall0(SYS_GETPID);
}

uint32_t write(int32_t fd, const void *buf, uint32_t count){
	return _syscall3(SYS_WRITE, fd, buf, count);
}

/*申请size字节大小的内存，并返回结果*/
void *malloc(uint32_t size){
	return (void *)_syscall1(SYS_MALLOC, size);
}

/*释放ptr指向的内存*/
void free(void *ptr){
	_syscall1(SYS_FREE, ptr);
}

pid_t fork(void){
	return _syscall0(SYS_FORK);
}
