#ifndef __INODE__H
#define __INODE__H
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



/*inode结构*/
struct inode{
	uint32_t i_no; //inode扇区编号
	/*当此inode为文件时，i_size指文件大小，若为目录，i_size是指该目录下所有目录项大小之后*/
	uint32_t i_size;

	uint32_t i_open_cnts; //记录此文件被打开的次数
	bool write_deny; //写文件不能并干

	uint32_t i_sectors[13];
	struct list_elem inode_tag;
};


void inode_sync(struct partition *part, struct inode *inode, void *io_buf);
struct inode *inode_open(struct partition *part, uint32_t inode_no);
void inode_close(struct inode *inode);
void inode_init(uint32_t inode_no, struct inode *new_inode);

//
#endif

