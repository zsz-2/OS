#include "ide.h"
#include "file.h"
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
#include "super_block.h"
#include "file.h"
#include "fs.h"
#include "inode.h"

/*打开根目录*/
void open_root_dir(struct partition *part){
	root_dir.inode = inode_open(part, part->sb->root_inode_no);
	root_dir.dir_pos = 0;
}

/*在分区part上打开i结点为inode_no的目录并返回目录指针*/
struct dir* dir_open(struct partition *part, uint32_t inode_no){
	struct dir *pg_dir = (struct dir *)sys_malloc(sizeof(struct dir));
	pg_dir->inode = inode_open(part,inode_no);
	pg_dir->dir_pos = 0;
	return pg_dir;
}

/*在part分区的pgdir目录内寻找名为name的文件或目录*/
bool search_dir_entry(struct partition *part, struct dir * pdir ,\
		const char *name, struct dir_entry *dir_e){
	uint32_t block_cnt = 140; //12个直接块 + 128个间接块 = 140块
	uint32_t *all_blocks = (uint32_t *)sys_malloc(48 + 512);
	if(all_blocks == NULL){
		printk("search dir_entry: sys_malloc for all_blocks failed");
		return false;
	}
	uint32_t block_idx = 0;
	while(block_idx < 12){
		all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
		++block_idx;
	}
	block_idx  = 0;
	if(pdir->inode->i_sectors[12] != 0){
		//存在一级间接表
		ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
	}
	/*all_blocks存储的是该文件或目录的所有扇区地址`*/


	/*写目录项是保证目录项不跨扇区，这样读目录项时容易处理，只申请一个扇区的内存*/
	uint8_t *buf  =  (uint8_t *)sys_malloc(SECTOR_SIZE);

	struct dir_entry *p_de = (struct dir_entry *)buf;
	//p_de为指向目录项的指针，值为buf的起始地址
	uint32_t dir_entry_size = part->sb->dir_entry_size;
	//1扇区内可容纳的目录项数目
	uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;


	/*在所有块中开始查找目录项目*/
	while(block_idx < block_cnt){
		//块地址为0时，表示块中无数据，继续在其他快中寻找
		if(all_blocks[block_idx] == 0){
			++block_idx;
			continue;
		}
		ide_read(part->my_disk, all_blocks[block_idx], buf, 1);

		uint32_t dir_entry_idx = 0;
		/*遍历扇区中的所有目录项*/
		while(dir_entry_idx < dir_entry_cnt){
			/*若找到了，则直接复制整个目录项*/
			if(strcmp(p_de->filename, name) == 0){
				memcpy(dir_e, p_de, dir_entry_size);
				sys_free(buf);
				sys_free(all_blocks);
				return true;
			}
			++dir_entry_idx;
			++p_de;
		}
		++block_idx;
		p_de = (struct dir_entry *) buf;
		//此时p_de已经指向扇区内最后一个完整的目录项
		//需要恢复p_de指向的buf
		memset(buf, 0, SECTOR_SIZE); //将buf清0，下次再用
	}
	sys_free(buf);
	sys_free(all_blocks);
	return false;
}

//关闭目录
void dir_close(struct dir *dir){
	//根目录不应该关闭
	//root_dir所在的内存在低端1MB之内，并非堆中，free会出问题
	if(dir == &root_dir){
		/*不做任何处理直接返回*/
		return;
	}
	inode_close(dir->inode);
	sys_free(dir);
}

//在内存初始化目录项p_de
void create_dir_entry(char *filename, uint32_t inode_no, \
		uint8_t file_type, struct dir_entry *p_de){
	ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);
	/*初始化目录项*/
	memcpy(p_de->filename, filename, strlen(filename));
	p_de->i_no = inode_no;
	p_de->f_type = file_type;
}

//将目录项p_de写入父目录parent_dir中，io_buf由主函数调用
bool sync_dir_entry(struct dir *parent_dir, \
		struct dir_entry *p_de, void *io_buf){
	struct inode *dir_inode = parent_dir->inode;
	uint32_t dir_size = dir_inode->i_size;
	uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

	//dir_size应该是dir_entry_size的整数倍
	ASSERT(dir_size % dir_entry_size == 0);

	//每扇区的最大目录项数目
	uint32_t dir_entrys_per_sec = (512 / dir_entry_size);
	
	int32_t block_lba = -1;

	/*将该目录的所有扇区地址存入all_blocks中*/
	uint8_t block_idx = 0;
	uint32_t all_blocks[140] = {0}; //all_blocks保存目录中的所有块

	/*将12个块直接存入all_blocks*/
	while(block_idx < 12){
		all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
		++block_idx;
	}

	//dir_e用来在io_buf中遍历目录项
	struct dir_entry *dir_e = (struct dir_entry*)io_buf;
	int32_t block_bitmap_idx = -1;

	/*开始遍历所有块以寻找目录项空位，若已有扇区没有空闲位
	 * 在不超过文件大小的情况下，申请新的扇区来存储新的目录项*/
	block_idx = 0;
	while(block_idx < 140){
		block_bitmap_idx = -1;
		if(all_blocks[block_idx] == 0){ //在3种情况下分配块
			block_lba = block_bitmap_alloc(cur_part);
			if(block_lba == -1){
				printk("alloc block bitmap for sync dir_entry failed");
				return false;
			}
			/*每分配一次块就同步一次block_bitmap*/
			block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
			ASSERT(block_bitmap_idx != -1);
			bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
			
			block_bitmap_idx = -1;
			if(block_idx < 12){//若是直接块
				dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
			}else if(block_idx == 12){
				//若是尚未分配的一级间接表
				dir_inode->i_sectors[12] = block_lba;
				//将上面分配的块作为一级间接块地址
				block_lba = -1;
				//再分配一个块作为一个间接块
				block_lba = block_bitmap_alloc(cur_part);
				if(block_lba == -1){
					block_bitmap_idx = dir_inode->i_sectors[12] - \
						cur_part->sb->data_start_lba;
					bitmap_set(&cur_part->block_bitmap, \
							block_bitmap_idx, 0);
					dir_inode->i_sectors[12] = 0;
					printk("alloc block bitmap for sync_dir_entry failed\n");
					return false;
				}
				//每分配一次块就同步一次block_bitmap
				block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
				ASSERT(block_bitmap_idx != -1);
				bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
				all_blocks[12] = block_lba;
				/*把新分配的第0个间接快写入一级间接块表*/
				ide_write(cur_part->my_disk, \
					dir_inode->i_sectors[12], all_blocks + 12, 1);

			}else{
				all_blocks[block_idx] = block_lba;
				ide_write(cur_part->my_disk, \
					dir_inode->i_sectors[12], all_blocks + 12, 1);

			}
			//再将新目录项写入新分配的间接块
			memset(io_buf, 0, 512);
			memcpy(io_buf, p_de, dir_entry_size);
			ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf,1);
			dir_inode->i_size += dir_entry_size;
			return true;
		}
		/*若block_idx已存在，将其读入内存, 查找空目录项*/
		ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
		/*在扇区内查找空目录项*/
		uint8_t dir_entry_idx = 0;
		while(dir_entry_idx < dir_entrys_per_sec){
			if( (dir_e + dir_entry_idx)->f_type == FT_UNKNOWN ){
				//FT_UNKNOWN为0， 无论是初始化还是删除文件后
				memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
				ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
				dir_inode->i_size += dir_entry_size;
				return true;
			}
			++dir_entry_idx;
		}
		++block_idx;
	}
	printk("directory is full!!!!\n");
	return false;
}

/*把分区part目录pdir中编号为inode_no的目录项删除*/
bool delete_dir_entry(struct partition *part, struct dir *pdir, uint32_t inode_no, void *io_buf){
	struct inode *dir_inode = pdir->inode;
	uint32_t block_idx = 0;
	uint32_t *all_blocks = (uint32_t *)sys_malloc(BLOCK_SIZE + 48);
	/*收集目录全部块地址*/
	while(block_idx < 12){
		all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
		++block_idx;
	}
	if(dir_inode->i_sectors[12]){
		ide_read(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
	}

	/*目录项在存储时保证不会跨扇区*/
	uint32_t dir_entry_size = part->sb->dir_entry_size;
	//每扇区可容纳的的最大目录项数目
	uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);
	
	struct dir_entry *dir_e = (struct dir_entry *)io_buf;
	struct dir_entry *dir_entry_found = NULL;
	uint8_t dir_entry_idx, dir_entry_cnt;
	bool is_dir_first_block; //目录的第一个块

	/*遍历所有块，寻找目录项*/
	block_idx = 0;
	while(block_idx < 140){
		is_dir_first_block = false;
		if(all_blocks[block_idx] == 0){
			++block_idx;
			continue;
		}
		dir_entry_idx = dir_entry_cnt = 0;
		memset(io_buf, 0, SECTOR_SIZE);
		/*读取扇区获得目录项*/
		ide_read(part->my_disk, all_blocks[block_idx], io_buf, 1);

		/*遍历所有的目录项，统计该扇区的目录项数目及是否有待删除的目录项*/
		while(dir_entry_idx < dir_entrys_per_sec){
			if((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN){
				if(strcmp((dir_e + dir_entry_idx)->filename, ".") == 0){
					is_dir_first_block = true;
				}else if(strcmp( (dir_e + dir_entry_idx)->filename, ".") !=0 &&\
				      strcmp( (dir_e + dir_entry_idx)->filename, ".."  ) != 0	){
					++dir_entry_cnt;
					//统计此扇区内的目录项个数，用来判断删除目录项后是否回收扇区
					if((dir_e + dir_entry_idx)->i_no == inode_no){
						//如果找到此i结点，就将其记录在dir_entry_find
						ASSERT(dir_entry_found == NULL);
						//确保目录中只有一个编号为inode_no的inode
						//找到一次后dir_entry_found就不该为NULL
						dir_entry_found = dir_e + dir_entry_idx;
					}
				}
			}
			++dir_entry_idx;
		}
		/*若该扇区未找到目录项，则去下一扇区寻找*/
		if(dir_entry_found == NULL){
			++block_idx;
			continue;
		}		

		/*在找到目录项后，清楚该目录项并判断是否回收扇区
		 * 随即退出循环后直接返回*/
		ASSERT(dir_entry_cnt >= 1);
		if(dir_entry_cnt == 1 && !is_dir_first_block){
			/*a 在位图中回收该块*/
			uint32_t block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
			bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
			bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

			/*将块地址从i_sectors位图中抹去*/
			if(block_idx < 12){
				dir_inode->i_sectors[block_idx]  = 0;
			}else{ //在一级间接块索引表中擦除该间接块的地址
				//先判断一级间接块索引表中间接块的数量，如果仅有这一个间接块，则将一级间接块也回收
				uint32_t indirect_blocks = 0;
				uint32_t indirect_block_idx = 12;
				while(indirect_block_idx < 140){
					if(all_blocks[indirect_block_idx] != 0){
						++indirect_blocks;
					}
					++indirect_block_idx;
				}
				ASSERT(indirect_blocks >= 1);  
				if(indirect_block_idx > 1){
					//间接索引表中还包括其他间接块，仅在索引表中擦除当前这个间接块地址
					all_blocks[block_idx] = 0;
					ide_write(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
				}else{ //间接索引表中就一个间接块
					//直接把间接索引表所在的块回收，然后擦除间接索引表块地址
					/*回收间接索引表所在的块*/
					block_bitmap_idx = dir_inode->i_sectors[12] - part->sb->data_start_lba;
					bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
					bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
					/*将间接索引表地址清0*/
					dir_inode->i_sectors[12] =  0;
				}
			}
		}else{  //仅将该目录项清空
			memset(dir_entry_found, 0, dir_entry_size);
			ide_write(part->my_disk, all_blocks[block_idx], io_buf, 1);
		}

		/*更新i结点信息并同步到硬盘*/

		ASSERT(dir_inode->i_size >= dir_entry_size);
		dir_inode->i_size -= dir_entry_size;
		memset(io_buf, 0, SECTOR_SIZE * 2 );
		inode_sync(part, dir_inode, io_buf);
		sys_free(all_blocks);
		return true;
	}
	/*所有块中未找到则返回false,出现这种情况应该是search file出错了*/
	sys_free(all_blocks);
	return false;
}

/*读取目录，成功返回1个目录项，失败返回NULL*/
struct dir_entry* dir_read(struct dir *dir){
	struct dir_entry *dir_e = (struct dir_entry *) dir->dir_buf;
	struct inode *dir_inode = dir->inode;
	uint32_t *all_blocks = (uint32_t *)sys_malloc(BLOCK_SIZE + 48);
	uint32_t block_cnt = 12, block_idx = 0, dir_entry_idx = 0;
	while(block_idx < 12){
		all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
		++block_idx;
	}
	if(dir_inode->i_sectors[12] != 0){ //若含有一级间接块表
		ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
		block_cnt = 140;
	}
	block_idx = 0;

	uint32_t cur_dir_entry_pos = 0;
	//当前目录项的偏移，此项用来判断是否是之前已经返回过的目录项
	uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
	uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;
	//1扇区内可容纳的目录项个数
	/*在目录大小内遍历*/
	while(dir->dir_pos < dir_inode->i_size){
		if(dir->dir_pos >= dir_inode->i_size){
			return NULL;
		}
		if(all_blocks[block_idx] == 0){
			//如果此块地址为0，即空块，继续读出下一块
			++block_idx;
			continue;
		}
		memset(dir_e, 0, SECTOR_SIZE);
		ide_read(cur_part->my_disk, all_blocks[block_idx], dir_e, 1);
		dir_entry_idx = 0;
		/*遍历扇区内所有的目录项*/
		while(dir_entry_idx < dir_entrys_per_sec){
			if((dir_e + dir_entry_idx)->f_type){
				//如果f_type不等于0，即不等于FT_UNKNOWN
				//判断是不是最新的目录项，避免返回曾经已经返回过的目录项
				if(cur_dir_entry_pos < dir->dir_pos){
					cur_dir_entry_pos += dir_entry_size;
					++dir_entry_idx;
					continue;
				}
				ASSERT(cur_dir_entry_pos == dir->dir_pos);
				dir->dir_pos += dir_entry_size;
				return dir_e + dir_entry_idx;
			}
			++dir_entry_idx;
		}
		++block_idx;
	}
	sys_free(all_blocks);
	return NULL;
}

/*判断目录是否为空*/
bool dir_is_empty(struct dir *dir){
	struct inode *dir_inode = dir->inode;
	/*若目录下只有.和..这两个目录项，则目录为空*/
	return (dir_inode->i_size == cur_part->sb->dir_entry_size * 2);
}

/*在父目录parent_dir中删除child_dir*/
int32_t dir_remove(struct dir* parent_dir, struct dir *child_dir){
	struct inode *child_dir_inode = child_dir->inode;
	/*空目录只在inode->i_sectors[0]中有扇区，其他扇区都该为空*/
	int32_t block_idx = 1;
	while(block_idx < 13){
		ASSERT(child_dir_inode->i_sectors[block_idx] == 0);
		++block_idx;
	}
	
	void *io_buf = sys_malloc(SECTOR_SIZE * 2);
	if(io_buf == NULL){
		printk("dir_remove: malloc for io_buf failed\n");
		return -1;
	}

	/*在父目录parent_dir中删除子目录child_dir对应的目录项*/
	delete_dir_entry(cur_part, parent_dir, child_dir_inode->i_no, io_buf);

	/*回收Inode中i_sectors所占的扇区*/
	inode_release(cur_part, child_dir_inode->i_no);
	sys_free(io_buf);
	return 0;
}
