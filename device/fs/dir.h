#ifndef __DIR__H
#define __DIR__H
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
#include "inode.h"
#include "fs.h"

#define MAX_FILE_NAME_LEN 16

struct dir root_dir;

/*目录结构*/
struct dir{
	struct inode *inode;
	uint32_t dir_pos; //记录在目录中的偏移
	uint8_t dir_buf[512]; //目录中的数据缓存
};

/*目录项结构*/
struct dir_entry{
	char filename[MAX_FILE_NAME_LEN];
	uint32_t i_no;  //普通文件或目录文件对应的inode编号
	enum file_types f_type;  //文件类型
};

#endif
