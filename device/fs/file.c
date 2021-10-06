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
#include "file.h"


/*从文件表file_table中获取一个空闲位，成功返回下标，失败返回-1*/
int32_t get_free_slot_in_global(void){
	uint32_t fd_idx = 3;
	while(fd_idx < MAX_FILE_OPEN){
		if(file_table[fd_idx].fd_inode == NULL) break;
		++fd_idx;
	}
	if(fd_idx == MAX_FILE_OPEN){
		printk("exceed max open files\n");
		return -1;
	}
	return fd_idx;
}

/**/
int32_t pcb_fd_install(int32_t global_fd_idx){
	struct task_struct *cur = running_thread();
	uint8_t local_fd_idx = 3; // 跨过stdin, stdout, stderr
	while(local_fd_idx < MAX_FILES_OPEN_PER_PROC){
		//-1表示free_slot，可用
		if(cur->fd_table[local_fd_idx] == -1){
			cur->fd_table[local_fd_idx] = global_fd_idx;
			break;
		}
		++local_fd_idx;
	}
	if(local_fd_idx == MAX_FILES_OPEN_PER_PROC){
		printk("exceed max open files_per_proc\n");
		return -1;
	}
	return local_fd_idx;
}

/*分配一个i结点，返回i结点号*/
int32_t inode_bitmap_alloc(struct partition *part){
	int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
	if(bit_idx == -1) return -1;
	bitmap_set(&part->inode_bitmap, bit_idx, 1);
	return bit_idx;
}

/*分配一个扇区，并返回扇区的起始地址*/
int32_t block_bitmap_alloc(struct partition *part){
	int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
	if(bit_idx == -1) return -1;
	bitmap_set(&part->block_bitmap, bit_idx, 1);
	/*返回具体可用的扇区地址*/
	return (part->sb->data_start_lba + bit_idx);
}

/*将内存bitmap中第bit_idx位所在的512字节同步到硬盘o*/
void bitmap_sync(struct partition *part, uint32_t bit_idx, uint8_t btmp){
	//本i结点索引相对于位图的扇区偏移量
	uint32_t off_sec = bit_idx / 4096;
	//本i结点索引相对于位图的字节偏移量
	uint32_t off_size = off_sec * BLOCK_SIZE;

	uint32_t sec_lba;
	uint8_t *bitmap_off;

	/*需要被同步到硬盘的只有inode_bitmap和block_bitmap*/
	switch(btmp){
		case INODE_BITMAP:
			sec_lba = part->sb->inode_bitmap_lba +  off_sec;
			bitmap_off = part->inode_bitmap.bits + off_size;
			break;
		case BLOCK_BITMAP:
			sec_lba = part->sb->block_bitmap_lba  + off_sec;
			bitmap_off = part->block_bitmap.bits + off_size;
			break;
	}
	ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

/*打开编号为inode_no的inode对应的文件，若成功返回文件描述符，否则返回-1*/
int32_t file_open(uint32_t inode_no, uint8_t flag){
	int fd_idx = get_free_slot_in_global();
	if(fd_idx == -1){
		printk("exceed max open files\n");
		return -1;
	}
	file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
	//每次打开文件，要将fd_pos还原为0，即让文件内的指针指向开头
	file_table[fd_idx].fd_pos = 0;	
	file_table[fd_idx].fd_flag = flag;
	
	bool *write_deny = &file_table[fd_idx].fd_inode->write_deny;
	
	if(flag & O_WRONLY || flag & O_RDWR){
		//只要是关于写文件，判断是否有其他进程正在写文件
		//若是读文件，不考虑write_deny
		//以下进入临界区先关中断
		enum intr_status old_status = intr_disable();
		if(!(*write_deny)){         //若当前没有其他进程写该文件，将其占用并置为true，避免多个进程同时写此文件
			*write_deny = true;
			intr_set_status(old_status);//恢复中断
		}else{
			intr_set_status(old_status);
			printk("file can't be write now, try again later\n");
			return -1;
		}
	}//若是读文件或者创建文件，不用理会
	return pcb_fd_install(fd_idx);
}

/*关闭文件*/
int32_t file_close(struct file *file){
	if(file == NULL) return -1;
	file->fd_inode->write_deny = false;
	inode_close(file->fd_inode);
	file->fd_inode = NULL; //使得文件结构可用
	return 0;
}


/*把buf中的count个字节写入file, 成功则返回写入的字节数，失败则返回-1*/
int32_t file_write(struct file *file, const  void *buf, uint32_t count){
	if( (file->fd_inode->i_size + count) > (BLOCK_SIZE * 140)){
		//文件目前最大只支持512 * 140 = 71680字节
		printk("exceed max file size 71680 bytes, write file failed\n");
		return -1;
	} 
	uint8_t *io_buf = sys_malloc(512);
	if(io_buf == NULL){
		printk("file write: sys_malloc for io_buf failed\n");
		return -1;
	}
	
	//用来记录文件所有的块地址
	uint32_t *all_blocks = (uint32_t *)sys_malloc(BLOCK_SIZE + 48);
	if(all_blocks == NULL){
		printk("file write: sys_malloc for all_blocks failed\n");
		return -1;
	}

	const uint8_t *src = buf; //用src指向buf中待写入的数据
	uint32_t bytes_written = 0; //用来记录已写入数据的大小
	uint32_t size_left = count; //用来记录仪未写入的数据大小
	int32_t block_lba = -1; //块地址
	//用来记录block对应于block_bitmap中的索引，作为参数传给bitmap_sync
	uint32_t block_bitmap_idx = 0;

	uint32_t sec_idx; //用来索引扇区
	uint32_t sec_lba; //扇区地址
	uint32_t sec_off_bytes; //扇区内字节偏移量
	uint32_t sec_left_bytes; //扇区内剩余字节量
	uint32_t chunk_size; //每次写入硬盘的数据块大小
	int32_t indirect_block_table; //用来获取一级间接表地址
	uint32_t block_idx; //块索引

	/*判断文件是否是第一次写，如果是，分配一个块*/
	if(file->fd_inode->i_sectors[0] == 0){
		block_lba =  block_bitmap_alloc(cur_part);
		if(block_lba == -1){
			printk("file_write: sys_malloc for alloc_blocks failed\n");
			return -1;
		}
		file->fd_inode->i_sectors[0] = block_lba;
		/*每分配一个快就将位图同步到硬盘上去*/
		block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
		ASSERT(block_bitmap_idx != 0);
		bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
	}

	/*写入count个字节前，该文件已经占用的块数*/
	uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;

	/*存储count字节后，该文件将占用的块数*/
	uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;

	ASSERT(file_will_use_blocks <= 140);

	/*通过此增量判断是否需要分配扇区,如果增量为0，表示原扇区够用*/
	uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;
	
	/*将文件所用到的块地址收集到all_blocks，系统块大小等于扇区大小*/
	if(add_blocks == 0){
		//在同一扇区写入数据，不涉及分配新的扇区
		if(file_will_use_blocks <= 12){
			//文件块数量在12之内
			block_idx = file_has_used_blocks-  1;
			//指向最后一个已有数据的扇区
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
		}else{
			/*未写入新数据之前已经占用了间接快，需要将间接快地址读进来*/
			ASSERT(file->fd_inode->i_sectors[12] != 0);
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12,1);
		}
	}else{
		/*若有增量，便涉及到分配新扇区是否分配一级间接块表,下面分3种情况处理*/
		if(file_will_use_blocks <= 12){
			/*现将有剩余空间的可继续用的扇区地址写入all_blocks*/
			block_idx = file_has_used_blocks - 1;
			ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

			/*再将未来要用的扇区分配好后写入all_blocks*/
			block_idx = file_has_used_blocks;// 指向第一个要分配的扇区
			while(block_idx < file_will_use_blocks){
				block_lba = block_bitmap_alloc(cur_part);
				if(block_lba == -1){
					printk("file write: block_bitmap_alloc for situation 1 failed\n");
					return -1;
				}
				//写文件时，不应该存在块未使用，但已经分配扇区的情况，当文件删除，把块地址清0
				ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
				//确保尚未分配扇区地址
				file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
				/*每分配一个快就把位图同步到硬盘*/
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
				++block_idx; //下一个分配的新扇区
			}
		}else if(file_has_used_blocks <= 12 && file_will_use_blocks > 12){
			/*第二种情况：旧数据在12个直接块内，新数据将使用间接块*/
			/*先将有剩余空间可继续用的扇区地址收集到all_blocks*/
			block_idx = file_has_used_blocks - 1;
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
			/*创建一级间接块表*/
			block_lba = block_bitmap_alloc(cur_part);
			if(block_lba == -1){
				printk("file_write: block_bitmap_alloc for situation 2 failed\n");
				return -1;
			}
			ASSERT(file->fd_inode->i_sectors[12] == 0);
			/*分配一级间接块索引表*/
			indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;
			
			//第一个未使用的块
			block_idx = file_has_used_blocks;

			while(block_idx < file_will_use_blocks){
				block_lba = block_bitmap_alloc(cur_part);
				if(block_lba == -1){
					printk("file_write: block_bitmap alloc for situation 2 failed\n");
					return -1;
				}
				if(block_idx < 12){
					//x新创建的0~11块直接存入all_blocks数组
					ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
					//确保尚未分配扇区地址
					file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
				}else{
					all_blocks[block_idx] = block_lba;
				}
				/*每分配一个块就同步到硬盘*/
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
				++block_idx;
			}
			ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1); //同步一级间接块到硬盘
		}else if(file_has_used_blocks > 12){
			/*第三种情况，新数据占据数据块*/
			ASSERT(file->fd_inode->i_sectors[12] != 0);
			//已经具备一级间接块表
			//获取一级间接表地址
			indirect_block_table = file->fd_inode->i_sectors[12];
			/*已使用的间接块也将被读入all_blocks,无需单独收录*/
			ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1); //获取所有间接块地址
			
			block_idx = file_has_used_blocks;

			while(block_idx < file_will_use_blocks){
				block_lba = block_bitmap_alloc(cur_part);
				if(block_lba == -1){
					printk("file_write block_bitmap_alloc for situation 3 failed\n");
					return -1;
				}
				all_blocks[block_idx++] = block_lba;

				/*每分配一个块就同步到硬盘*/
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
			}
			//同步一级间接块表到硬盘
			ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
		}
	}
	/*用到的块地址已经收集到all_blocks中,下面开始写数据*/
	bool first_write_block = true;
	file->fd_pos = file->fd_inode->i_size - 1;
	while(bytes_written < count){
		memset(io_buf, 0, BLOCK_SIZE);
		sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
		sec_lba = all_blocks[sec_idx];
		sec_off_bytes = file->fd_inode->i_size %  BLOCK_SIZE;
		sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

		/*判断此次写入硬盘的的数据大小*/
		chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
		if(first_write_block){
			ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
			first_write_block = false;
		}
		memcpy(io_buf + sec_off_bytes, src, chunk_size);
		ide_write(cur_part->my_disk, sec_lba, io_buf, 1);
		printk("file write at lba 0x%x\n",sec_lba);
		src += chunk_size;
		file->fd_inode->i_size += chunk_size; //更新文件大小
		file->fd_pos += chunk_size;
		bytes_written += chunk_size;
		size_left -= chunk_size;
	}
	inode_sync(cur_part, file->fd_inode, io_buf);
	sys_free(all_blocks);
	sys_free(io_buf);
	return bytes_written;
}

/*从文件file中读取count个字节写入buf,返回读出的字节数，若到文件尾返回-1*/
int32_t file_read(struct file *file,  void *buf, uint32_t count){
	uint8_t *buf_dst = (uint8_t *)buf;
	uint32_t size = count, size_left = size;

	/*若要读取的字节数超过了文件可读的剩余量，则剩余量为待读取的字节数*/
	if((file->fd_pos + count) > file->fd_inode->i_size){
		size = file->fd_inode->i_size - file->fd_pos;
		size_left = size;
		if(size == 0){ //若到文件尾则返回-1
			return -1;
		}
	}

	uint8_t *io_buf = sys_malloc(BLOCK_SIZE);
	if(io_buf == NULL){
		printk("file_read: sys_malloc for io_buf failed\n");
		return -1;
	}
	//用来记录文件的所有块地址
	uint32_t *all_blocks = (uint32_t *)sys_malloc(BLOCK_SIZE + 48);

	if(all_blocks == NULL){
		printk("file_read: sys_malloc for all_blocks failed\n");
		return -1;
	}

	//数据块所在的起始地址和终止地址
	uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;
	uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;

	uint32_t read_blocks = block_read_end_idx - block_read_start_idx; //如果增量为0，表示数据在同一扇区
	ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);

	int32_t indirect_block_table; //用来获取一级间接表地址
	uint32_t block_idx; //获取待读的块地址

	/*以下开始构建all_blocks块地址数组，专门存储用到的块地址*/
	if(read_blocks == 0){//在同一扇区内读数据，不涉及跨扇区读取
		ASSERT(block_read_end_idx == block_read_start_idx);
		if(block_read_end_idx < 12){ //待读的数据在12个直接块内
			block_idx = block_read_end_idx;
			all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
		}else{ //用到了一级间接块表，需要将表中间块读进来
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
		}
	}else{
		/*第一种情况起始块和终止快属于直接块*/
		if(block_read_end_idx < 12){
			block_idx = block_read_start_idx;
			while(block_idx <= block_read_end_idx){
				all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
				++block_idx;
			}
		}else if(block_read_start_idx < 12 && block_read_end_idx >= 12){
			/*待读入的数据跨越直接块和间接块两类*/
			block_idx = block_read_start_idx;
			while(block_idx < 12){
				all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
				++block_idx;
			}
			//确保已经分配一级间接块表
			ASSERT(file->fd_inode->i_sectors[12] != 0);

			/*再将间接块地址写入all_blocks中*/
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk,  indirect_block_table, all_blocks + 12, 1 );
		}else{
			//确保已经分配一级间接块表
			ASSERT(file->fd_inode->i_sectors[12] != 0);

			/*再将间接块地址写入all_blocks中*/
			indirect_block_table = file->fd_inode->i_sectors[12];
			ide_read(cur_part->my_disk,  indirect_block_table, all_blocks + 12, 1 );	
		}
	}

	/*用到的块地址已经收集到all_blocks中，下面开始读数据*/
	uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
	uint32_t bytes_read = 0;
	while(bytes_read < size){ //直到读完为止
		sec_idx = file->fd_pos / BLOCK_SIZE;
		sec_lba = all_blocks[sec_idx];
		sec_off_bytes = file->fd_pos % BLOCK_SIZE;
		sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
		chunk_size = size_left < sec_left_bytes ? size_left: sec_left_bytes; //待读入数据的大小

		memset(io_buf, 0, BLOCK_SIZE); 
		ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
		memcpy(buf_dst, io_buf + sec_off_bytes, chunk_size);

		buf_dst += chunk_size;
		file->fd_pos += chunk_size;
		bytes_read += chunk_size;
		size_left -= chunk_size;
	}
	sys_free(all_blocks);
	printk("hahahhahaha\n");
	sys_free(io_buf);
	return bytes_read;

}
