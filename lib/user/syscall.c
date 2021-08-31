#include "syscall.h"

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
	:"=a"(retval))\
	:"0"(NUMBER),"b"(ARG1), "c"(ARG2) \
	:"memory");\
	retval;\
})

/*3参数的系统调用*/
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({\
	int retval;\
	asm volatile( \
	"int $0x80" \
	:"=a"(retval))\
	:"0"(NUMBER),"b"(ARG1), "c"(ARG2), "d"(ARG3) \
	:"memory");\
	retval;\
})


/*返回当前任务的pid*/
uint32_t getpid(){
	//while(1);
	return _syscall0(SYS_GETPID);
}
