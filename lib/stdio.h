#ifndef __STDIO_H
#define __STDIO_H
#include "stdio.h"
#include "syscall.h"
#include "stdint.h"
#include "string.h"

typedef char* va_list;
uint32_t printf(const char *format, ...);
#endif
