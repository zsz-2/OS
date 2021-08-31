#ifndef __SYSCALL_INIT_H
#define __SYSCALL_INIT_H
#include "thread.h"
#include "stdint.h"
#include "global.h"
#include "interrupt.h"
#include "syscall.h"
#include "io.h"
void syscall_init(void);
#endif
