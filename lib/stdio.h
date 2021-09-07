#ifndef __STDIO_H
#define __STDIO_H
#include "syscall.h"
#include "stdint.h"
#include "string.h"


#define va_start(ap,v) ap= (va_list)&v  //把ap指向第一个固定参数v
#define va_arg(ap, t) *((t *)(ap += 4)) //ap指向下一个参数并返回其值
#define va_end(ap) ap =  NULL  //将ap置为NULL

typedef char* va_list;
uint32_t printf(const char *format, ...);
#endif
