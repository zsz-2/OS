#include "inode.h"
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

/*用来存储inode的位置*/
struct inode_position{
	bool two_sec;  //inode是否跨扇区
	uint32_t sec_lba; //inode所在的扇区号
	uint32_t off_size; //inode在扇区内的字节偏移量
};


/*获取inode所在的扇区和扇区内的偏移量*/
static void inode_locate(struct partition *part, uint32_t inode_no, struct inode_position *inode_pos){
	/*inode_table在硬盘是是连续的*/
	ASSERT(inode_no < 4096);
	uint32_t inode_table_lba = part->sb->inode_table_lba;

	uint32_t inode_size = sizeof(struct inode);
	//inode的字节偏移量
	uint32_t off_size = inode_no * inode_size;
	//inode的扇区偏移量
	uint32_t off_sec = off_size / 512;
	//inode的起始地址
	uint32_t off_size_in_sec = off_size % 512;

	/*判断inode是否跨越两个扇区*/
	uint32_t left_in_sec = 512 - off_size_in_sec;
	if(left_in_sec < inode_size){
		//不足以容纳一个inode，必然跨越两个扇区
		inode_pos->two_sec = true;
	}else{
		inode_pos->two_sec = false;
	}
	inode_pos->sec_lba = inode_table_lba + off_sec;
	inode_pos->off_size = off_size_in_sec;
}

/*将inode写入分区part*/
void inode_sync(struct partition *part, struct inode *inode, void *io_buf){
	//io_buf是用于硬盘io的缓冲区
	uint8_t inode_no = inode->i_no;
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);

	//inode信息会存入inode_pos
	ASSERT(inode_pos.sec_lba <= part->start_lba + part->sec_cnt);

	struct inode pure_inode;
	memcpy(&pure_inode, inode, sizeof(struct inode));

	/*以下inode的成员只存在于内存中，将inode同步到硬盘，清除掉即可 */
	pure_inode.i_open_cnts = 0;
	pure_inode.write_deny = false;
	pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

	char *inode_buf = (char *)io_buf;
	if(inode_pos.two_sec){
		//若是跨越了两个扇区，就读出两个扇区，再写两个扇区
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
		memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
		/*将拼接好的数据再写入磁盘*/
		ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
	}else{
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
		memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
		ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
	}
}

/*根据i结点号返回相应的i结点*/
struct inode *inode_open(struct partition *part, uint32_t inode_no){
	/*先在已经打开的inode链表中找inode*/
	struct list_elem *elem = part->open_inodes.head.next;
	struct inode *inode_found;
	while(elem != &part->open_inodes.tail){
		inode_found = elem2entry(struct inode, inode_tag, elem);
		if(inode_found->i_no == inode_no){
			++inode_found->i_open_cnts;
			return inode_found;
		}
	}

	/*由于open_inodes在链表中找不到，从硬盘读取并加入链表中*/
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);

	/*为了让sys_malloc创建的新inode被所有任务共享，必须将inode创建到内核线程中*/
	struct task_struct *cur = running_thread();
	uint32_t *cur_pagedir_bak = cur->pgdir;
	cur->pgdir = NULL;

	/*以下三行代码完成后下面分配的内存将位于内核区*/
	inode_found = (struct inode *)sys_malloc(sizeof(struct inode));
	/*恢复pg_dir*/
	cur->pgdir = cur_pagedir_bak;

	char *inode_buf;
	if(inode_pos.two_sec){
		inode_buf = (char *)sys_malloc(1024);
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
	}else{
		inode_buf = (char *)sys_malloc(512);
		ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
	}

	memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));

	/*因为很可能即将用到该inode，所以直接将其插入队首*/
	list_push(&part->open_inodes, &inode_found->inode_tag);
	inode_found->i_open_cnts = 1;
	sys_free(inode_buf);
	return inode_found;
}

/*关闭inode或减少inode的打开数*/
void inode_close(struct inode *inode){
	/*若没有进程使用该文件，则将其删除*/
	enum intr_status old_status = intr_disable();
	if(--inode->i_open_cnts == 0){
		list_remove(&inode->inode_tag);
		/*释放内存时也要确保是内核内存池*/
		struct task_struct *cur = running_thread();
		uint32_t *cur_pagedir_bak = cur->pgdir;
		cur->pgdir = NULL;
		sys_free(inode);
		cur->pgdir = cur_pagedir_bak;
	}
	intr_set_status(old_status);
}


/*初始化new_inode*/
void inode_init(uint32_t inode_no, struct inode *new_inode){
	new_inode->i_no = inode_no;
	new_inode->i_size = 0;
	new_inode->i_open_cnts = 0;
	new_inode->write_deny = false;

	/*初始化索引数组i_sector*/
	uint8_t sec_idx;
	while(sec_idx < 13){
		new_inode->i_sectors[sec_idx] =  0;
		++sec_idx;
	}
}
