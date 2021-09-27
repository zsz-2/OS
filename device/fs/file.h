#ifndef __FILE__H
#define __FILE__H
#include "ide.h"
#include "interrupt.h"
#include "timer.h"
#include "stdio-kernel.h"
#include "io.h"
#include "global.h"
#include "list.h"
#include "stdint.h"
#include "bitmap.h"
#include "sync.h"
#include "dir.h"
#include "fs.h"
#include "super_block.h"

/*文件结构*/
struct file{
	//记录文件的偏移地址
	uint32_t fd_pos;
	uint32_t fd_flag;
	struct inode *fd_inode;
};

/*标准输入输出描述符*/
enum std_fd{
	stdin_no, //0标准输入
	stdout_no, //1标准输出
	stderr_no //2标准错误
};

/*位图类型*/
enum bitmap_type{
	INODE_BITMAP, //inode位图
	BLOCK_BITMAP //块位图
};

#define MAX_FILE_OPEN 32  //系统可打开的最大文件数
/*文件表*/
struct file file_table[MAX_FILE_OPEN];

int32_t get_free_slot_in_global(void);
int32_t pcb_fd_install(int32_t global_fd_idx);
int32_t inode_bitmap_alloc(struct partition *part);
int32_t block_bitmap_alloc(struct partition *part);
void bitmap_sync(struct partition *part, uint32_t bit_idx, uint8_t btmp);

#endif
