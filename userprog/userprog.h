#ifndef __KERNEL_USERPROG_H
#define __KERNEL_USERPROG_H
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)

#include "global.h"
#include "userprog.h"
#include "list.h"
#include "thread.h"
#include "io.h"
#include "stdint.h"

void start_process(void *);
void page_dir_activate(struct task_struct *);
void process_activate(struct task_struct *);
uint32_t *create_page_dir(void);
void create_user_vaddr_bitmap(struct task_struct *user_prog);
void process_execute(void* filename, char *name);
#endif
