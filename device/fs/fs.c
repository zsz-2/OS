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
#include "inode.h"
#include "super_block.h"


static void partition_format(struct disk *hd, struct partition *part){
	uint32_t boot_sector_sects = 1;
	uint32_t super_block_sects = 1;
	//I结点占位图占用的扇区数，最多支持4096个文件
	uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
	uint32_t inode_table_sects = DIV_ROUND_UP((sizeof(struct inode) * MAX_FILES_PER_PART),\
			SECTOR_SIZE);
	uint32_t used_sects = boot_sector_sects + super_block_sects + \
			      inode_bitmap_sects + inode_table_sects;
	uint32_t free_sects = part->sec_cnt - used_sects;

	/***********简单处理块位图占据的扇区数*******************/
	uint32_t block_bitmap_sects;
	block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
	/*可用块的数量*/
	uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
	block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

	/*超级块初始化*/
	struct super_block sb;
	sb.magic = 0x19990512;
	sb.sec_cnt = part->sec_cnt;
	sb.inode_cnt = MAX_FILES_PER_PART;
	sb.part_lba_base = part->start_lba;

	sb.block_bitmap_lba = sb.part_lba_base + 2;
	sb.block_bitmap_sects = block_bitmap_sects;

	sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
       	sb.inode_bitmap_sects = inode_bitmap_sects;
	
	sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
	sb.inode_table_sects = inode_table_sects;

	sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
	sb.root_inode_no = 0;
	sb.dir_entry_size = sizeof(struct dir_entry);

	printk("%s info:\n", part->name);
	printk("magic: %x\n  part_lba_base:%d\n all_sectors:%d\n  inode_cnt:%d\n block_bitmap_lba:%d\n block_bitmap_sectors:%d\n  inode_bitmap_lba:%d\n inode_bitmap_sectors:%d\n inode_table_lba:%d\n  inode_table_sectors:%d\n data_start_lba:%d\n", sb.magic,  sb.part_lba_base, sb.sec_cnt, \
			sb.inode_cnt, sb.block_bitmap_lba, sb.block_bitmap_sects, \
			sb.inode_bitmap_lba, sb.inode_bitmap_sects, \
			sb.inode_table_lba, sb.inode_table_sects, sb.data_start_lba);

	hd = part->my_disk;
	/*********1.将超级块写入本分区的1扇区****************/
	ide_write(hd, part->start_lba + 1, &sb, 1);
	printk("   super_block_lba: 0x%x\n", part->start_lba + 1);

	/*找出尺寸最大的元信息，用其尺寸做存储缓冲区*/
	uint32_t buf_size = (sb.block_bitmap_sects >= \
			sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects);
	buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;

	uint8_t *buf = (uint8_t *)sys_malloc(buf_size);

	/*将块位图初始化并写入sb.block_bitmap_lba*/
	buf[0] |= 0x01;  //第0块为根目录
	uint32_t block_bitmap_last_byte  = block_bitmap_bit_len / 8;
	uint8_t block_bitmap_last_bit =  block_bitmap_bit_len % 8;

	//last_size是位图所在最后一个扇区中，不足一扇区的其余部分
	uint32_t last_size = SECTOR_SIZE - (block_bitmap_bit_len % SECTOR_SIZE);
	
	/*先将位图最后一字节到其所在的扇区结束全部置为1*/
	memset(&buf[block_bitmap_last_byte], 0xff, last_size);

	/*再将上一步中覆盖的最后一字节内有效位重新置0*/
	uint8_t bit_idx = 0;
	while(bit_idx < block_bitmap_last_bit){
		buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
	}
	
	ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);

	/*将inode位图初始化并写入sb.inode_bitmap_lba,先清空缓冲区*/
	memset(buf, 0, buf_size);
	buf[0] |= 0x01;
	/*由于inode_table共4096个文件inode,inode_bitmap刚好占用一扇区，无需处理剩余部分*/
	ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);

	/*将inode数组初始化并写入inode_table_lba*/
	memset(buf, 0, buf_size);
	struct inode *i = (struct inode *)buf;
	i->i_size = sb.dir_entry_size * 2; //.和..
	i->i_no = 0; //根目录占inode数组中第0个inode
	i->i_sectors[0] = sb.data_start_lba;
	ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

	/*将根目录写入sb.data_start_lba*/
	memset(buf, 0, buf_size);
	struct dir_entry *p_de = (struct dir_entry*)buf;
	/*初始化当前目录*/
	memcpy(p_de->filename, ".", 1);
	p_de->i_no = 0;
	p_de->f_type = FT_DIRECTORY;
	p_de++;

	/*初始化当前父目录*/
	memcpy(p_de->filename, "..", 2);
	p_de->i_no = 0;
	p_de->f_type = FT_DIRECTORY;

	/*写入根目录*/
	ide_write(hd, sb.data_start_lba, buf, 1);

	printk("root_dir_lba:0x%x\n", sb.data_start_lba);
	printk("%s format done\n", part->name);
	sys_free(buf);
}

/*在磁盘上搜索文件系统，若没有则格式化分区创建文件系统*/
void filesys_init(){
	uint8_t channel_no = 0, dev_no, part_idx = 0;
	/*sb_buf用来存储从硬盘读入的超级块*/
	struct super_block *sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

	if(sb_buf == NULL){
		PANIC("alloc memory failed!!!");
	}
	printk("searching filesystem.......\n");
	while(channel_no < channel_cnt){
		dev_no = 0;
		while(dev_no < 2){
			if(dev_no == 0){
				++dev_no;
				continue;
			}
			struct disk *hd = &channels[channel_no].devices[dev_no];
			struct partition *part = hd->prim_parts;
			while(part_idx < 12){
				if(part_idx == 4){
					part = hd->logic_parts;
				}
				/*channels数组是全局变量，默认值是0，disk属于嵌套结构i
				 * partition为disk的嵌套结构，因此partition中成员默认为0*/
				if(part->sec_cnt !=0){
					memset(sb_buf, 0, SECTOR_SIZE);
	
					/*读出分区的超级块*/
					ide_read(hd, part->start_lba + 1, sb_buf, 1);

					/*判断是否为自己的文件系统*/
					if(sb_buf->magic == 0x19990512){
						printk("%s has filesystem\n", part->name);
					}else{
						printk("formating %s's partition%s......\n",\
								hd->name, part->name);
						partition_format(hd, part);
					}
				}
				++part_idx;
				++part;
			}
			++dev_no;
		}
		++channel_no;
	}
	sys_free(sb_buf);
}

/*在分区链表中找到名为part_name的分区，并将指针值赋给cur_part*/
static bool mount_partition(struct list_elem *pelem, int arg){
	char *part_name = (char *)arg;
	struct partition *part = elem2entry(struct partition, part_tag, pelem);
	if(strcmp(part->name, arg) == 0){
		cur_part = part;
		struct disk *hd = cur_part->my_disk;
		/*sb_buf用来存储硬盘读入的超级块*/
		struct super_block *sb_buf =  (struct super_block *)sys_malloc(SECTOR_SIZE);

		/*在内存中创建分区cur_part的超级块*/
		cur_part->sb = (struct super_block *)sys_malloc(sizeof(struct super_block));
		if(cur_part->sb == NULL){
			PANIC("alloc memory failed!!!");
		}
		
		/*读入超级块*/
		memset(sb_buf, 0, SECTOR_SIZE);
		ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);

		/*把sb_buf信息复制到超级块中*/
		memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));

		/*将硬盘的块位图读取到内存中*/
		cur_part->block_bitmap.bits = (uint8_t *)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
		if(cur_part->block_bitmap.bits == NULL){
			PANIC("alloc memory failed!!!\n");
		}
		cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;

		ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sects);

		/*将硬盘中的inode位图读取到硬盘中*/
		cur_part->inode_bitmap.bits = (uint8_t *)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
		if(cur_part->inode_bitmap.bits == NULL){
			PANIC("alloc memory failed!!!\n");
		}
		
		cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;

		ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_sects);

		list_init(&cur_part->open_inodes);
		printk("mount %s done\n", part->name);
		return true;
	}
	return false;
}


/*将最上层路径名解析出来*/
static char *path_parse(char *pathname, char *name_store){
	if(pathname[0] == '/'){ //根目录不需要单独解析
		//路径中出现1个或多个连续的字符'/',将这些'/'跳过，
		while(*(++pathname) == '/');
	}
	/*开始一般的路径解析*/
	while(*pathname != '/' && *pathname != 0){
		*name_store++ = *pathname++;
	}
	*name_store = 0;
	//若路径字符串位空，则返回NULL
	if(pathname[0] == 0) return NULL;
	return pathname;
}


/*返回路径深度*/
int32_t path_depth_cnt(char *pathname){
	ASSERT(pathname != NULL);
	char *p = pathname;
	//用于path_parse的参数路径解析
	char name[MAX_FILE_NAME_LEN];
	
	uint32_t depth = 0;
	
	/*解析路径，从中拆分出各级的名称*/
	p = path_parse(p, name);
	while(name[0]){
		++depth;
		memset(name, 0, MAX_FILE_NAME_LEN);
		//如果p不等于NULL，继续分析路径
		if(p){
			p = path_parse(p, name);
		}
	}
	return depth;
}

/*搜索文件pathname,找到则返回其inode号，否则返回-1*/
static int search_file(const char *pathname, struct path_search_record *searched_record){
	/*如果待查找的是根目录，直接返回已知根目录信息*/
	if(strcmp(pathname, "/") == 0 || strcmp(pathname, "/.") == 0 || strcmp(pathname, "/..") == 0){
		searched_record->parent_dir = &root_dir;
		searched_record->file_type = FT_DIRECTORY;
		searched_record->searched_path[0] = 0; //搜索路径设置为空
		return  0;
	}
	uint32_t path_len = strlen(pathname);
	//保证pathname至少是这样的路径: /x，且小于最大长度
	ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
	char *sub_path = (char *)pathname;
	struct dir *parent_dir = &root_dir;
	struct dir_entry dir_e;

	/*记录路径解析出来的各级名称，如路径/a/b/c,
	 * 数组name每次的值为a,b,c*/
	char name[MAX_FILE_NAME_LEN] = {0};

	searched_record->parent_dir = parent_dir;
	searched_record->file_type = FT_UNKNOWN;
	uint32_t parent_inode_no = 0; //父目录的inode号

	sub_path = path_parse(sub_path, name);
	while(name[0]){ //若第一个为结束符，则结束循环
		/*记录查找过的路径，但不能超过searched_path的长度512字节*/
		ASSERT(strlen(searched_record->searched_path) < 512);

		/*记录已存在的父目录*/
		strcat(searched_record->searched_path, "/");
		strcat(searched_record->searched_path, name);
		printk("%s\n", searched_record->searched_path);
		/*在所给的目录中查找文件*/
		if(search_dir_entry(cur_part, parent_dir, name, &dir_e)){
			memset(name, 0, MAX_FILE_NAME_LEN);
			/*若sub_path不为NULL，则继续拆分路径*/
			if(sub_path){
				sub_path = path_parse(sub_path, name);
			}
			printk("search file hahahha\n");
			if(FT_DIRECTORY == dir_e.f_type){ //如果被打开的是目录
				parent_inode_no = parent_dir->inode->i_no;
				dir_close(parent_dir);
				parent_dir = dir_open(cur_part, dir_e.i_no); //更新父目录
				searched_record->parent_dir =  parent_dir;
				continue;
			}else if(FT_REGULAR ==  dir_e.f_type){ //如果是普通文件
				searched_record->file_type = FT_REGULAR;
				return dir_e.i_no;
			}
		}else{   //若找不到，则返回-1
			/*若找不到目录项时，要留着parent_dir不要关闭
			 * 若是创建文件需要在parent_dir中创建*/
			return -1;
		}
	}
	
	//执行到此，必然是经过了完整的路径
	dir_close(searched_record->parent_dir);	
	/*保存被查找目录的直接父目录*/
	searched_record->parent_dir = dir_open(cur_part, parent_inode_no);
	searched_record->file_type = FT_DIRECTORY;
	return dir_e.i_no;
}

/*创建文件，若成功则返回文件描述符，否则返回-1*/
int32_t file_create(struct dir *parent_dir, char *filename, uint8_t flag){
	/*后续操作的公共缓冲区*/
	void *io_buf = sys_malloc(1024);
	if(io_buf == NULL){
		printk("in file_create: sys_malloc for io_buf failed\n");
		return -1;
	}

	uint8_t rollback_step = 0; //用于操作失败时回滚各资源状态

	/*为新文件分配inode*/
	int32_t inode_no = inode_bitmap_alloc(cur_part);
	if(inode_no == -1){
		printk("in file_create: allocate inode failed\n");
		return -1;
	}
	
	/*此inode要从堆中申请内存，不可生成局部变量(函数退出时会释放)
	 * 因为file_table数组中的文件描述符的inode指针要指向它*/
	struct inode *new_file_inode = \
		(struct inode *)sys_malloc(sizeof(struct inode));
	if(new_file_inode == NULL){
		printk("file_create: sys_malloc for inode failed\n");
		rollback_step = 1;
		goto rollback;
	}
	inode_init(inode_no, new_file_inode); //初始化i结点

	/*返回的是file_table数组的下标*/
	int fd_idx = get_free_slot_in_global();
	if(fd_idx == -1){
		printk("exceed max open files\n");
		rollback_step = 2;
		goto rollback;
	}
	

	file_table[fd_idx].fd_inode = new_file_inode;
	file_table[fd_idx].fd_pos = 0;
	file_table[fd_idx].fd_flag = flag;
	file_table[fd_idx].fd_inode->write_deny = false;

	struct dir_entry new_dir_entry;
	memset(&new_dir_entry, 0, sizeof(struct dir_entry));

	//只要内存不出错，该函数不会报错
	create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

	/*同步数据到硬盘*/	
	/*在目录下安装目录项new_dir_entry*/
	if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)){
		printk("sync dir_entry to disk failed\n");
		rollback_step = 3;
		goto rollback;
	}

	memset(io_buf, 0, 1024);
	//将父目录i节点的内容同步到硬盘上
	inode_sync(cur_part, parent_dir->inode, io_buf);
	//将新创建文件的i结点同步到
	memset(io_buf, 0, 1024);
	inode_sync(cur_part, new_file_inode , io_buf);

	//将inode_bitmap位图同步到硬盘
	bitmap_sync(cur_part, inode_no, INODE_BITMAP);

	//将创建的文件i结点添加到open_inodes链表
	list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
	new_file_inode->i_open_cnts = 1;

	sys_free(io_buf);
	return pcb_fd_install(fd_idx);

	/*创建文件需要创建相关的多个资源，若某步失败则会执行到下面的回滚步骤*/
rollback:
	switch (rollback_step){
		case 3:
			/*失败时，将file_table中的相应位清空*/
			memset(&file_table[fd_idx], 0, sizeof(struct file));
		case 2:
			sys_free(new_file_inode);
		case 1:
			/*若新文件的i结点创建失败，之前位图分配的inode_no也要恢复*/
			bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
			break;
	}
	sys_free(io_buf);
	return -1;
}

//打开或创建文件成功后，返回文件描述符，否则返回-1
int32_t sys_open(const char *pathname, uint8_t flag){
	//对目录要用dir_open，这里只open文件
	if(pathname[strlen(pathname) - 1] == '/'){
		printk("can't open a directory %s\n", pathname);
		return -1;
	}
	ASSERT(flag <= 7);
	int32_t fd = -1; //默认为找不到

	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));

	/*记录目录深度，帮助判断中间某个目录不存在的情况*/
	uint32_t pathname_depth = path_depth_cnt((char *)pathname);
	
	/*先检查文件是否存在*/
	int inode_no = search_file(pathname, &searched_record);
	bool found = (inode_no != -1 ? true :false);
	printk("foud: %d\n", found);

	if(searched_record.file_type == FT_DIRECTORY){
		printk("can't open a directory with open(), use opendir() to instead\n");
		dir_close(searched_record.parent_dir);
		return -1;
	}

	uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);

	/*先判断是否把pathname的各层目录都访问到了
	 * 即是否在某个中间目录就失败了*/
	if(pathname_depth != path_searched_depth){
		//说明没有访问到全部的路径，某个中间目录是不存在的
		printk("cannot access %s: Not a directory, subpath %s is't exist\n", \
				pathname, searched_record.searched_path);
		dir_close(searched_record.parent_dir);
		return -1;
	}

	/*若是在最后一个路径上没找到，并且不是创建文件，直接返回-1*/
	if(!found && !(flag & O_CREAT)){
		printk("in path %s, file %s is't exist\n", \
				searched_record.searched_path, (strrchr(searched_record.searched_path, '/') + 1));
		dir_close(searched_record.parent_dir);
		return -1;
	}else if(found && (flag == O_CREAT)){
		printk("%s has already exist!\n", pathname);
		dir_close(searched_record.parent_dir);
		return -1;
	}

	switch (flag & O_CREAT){
		case O_CREAT:
			if(found){ 
				fd = file_open(inode_no, flag);
				return fd;
			}
			printk("creating file\n");
			fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1) , flag );
			dir_close(searched_record.parent_dir);
			break;
		/*其余情况均为打开已存在*/
		default:
			fd = file_open(inode_no, flag);
	}
	//此fd是指任务pcb->fd_table数组中的元素下标，并不是指全局file_table中的下标
	return fd;
}

/*将文件描述符转化为文件表的下标*/
static uint32_t fd_local2global(uint32_t local_fd){
	struct task_struct *cur = running_thread();
	int32_t global_fd = cur->fd_table[local_fd];
	ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
	return (uint32_t)global_fd;
}

/*关闭文件描述符fd指向的文件，成功返回0，失败返回-1*/
int32_t sys_close(int32_t fd){
	int32_t ret = -1; //返回值默认为-1。即失败
	if(fd > 2){
		uint32_t _fd = fd_local2global(fd);
		ret = file_close(&file_table[fd]);
		running_thread()->fd_table[fd] = -1; //使该文件描述符可用
	}
	return ret;
}


/*将buf中连续count字节写入文件描述符fd,成功返回写入的字节数，失败返回-1*/
int32_t sys_write(int32_t fd, const void *buf, uint32_t count){
	if(fd < 0){
		printk("sys_write: fd error\n");
		return -1;
	}
	if(fd == stdout_no){
		char tmp_buf[1024] = {0};
		memcpy(tmp_buf, buf, count);
		console_put_str(tmp_buf);
		return count;
	}
	uint32_t _fd = fd_local2global(fd);
	struct file * wr_file = &file_table[_fd];
	if(wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR){
		uint32_t bytes_written = file_write(wr_file, buf, count);
		return bytes_written;
	}else{
		console_put_str("sys_write: not allowed  to write file without flag O_RDWR or O_WRONLY\n");
		return -1;
	}

}

/*从文件描述符fd指向的文件中读取count个字节到buf,若成功则返回读取的字节数，到文件尾则返回-1*/
int32_t sys_read(int32_t fd, void *buf, uint32_t count){
	if(fd < 0){
		printk("sys_read: fd error\n");
		return -1;
	}
	ASSERT(buf != NULL);
	uint32_t _fd = fd_local2global(fd);
	return file_read(&file_table[_fd], buf, count);
}

/*重置文件读写操作的偏移指针，成功时返回新的偏移量，出错时返回-1*/
int32_t sys_lseek(int32_t fd, int32_t offset , uint8_t whence){
	if(fd < 0){
		printk("sys_lseek: fd error\n");
		return -1;
	}
	ASSERT(whence > 0 && whence < 4);
	uint32_t _fd = fd_local2global(fd);
	struct file *pf = &file_table[_fd];
	int32_t new_pos = 0; //新的偏移量必须位于文件大小之内
	int32_t file_size = (int32_t)pf->fd_inode->i_size;
	switch (whence){
		/*SEEK_SET 新的读写位置是相对于文件开头再增加offset个位移量*/
		case SEEK_SET:
			new_pos = offset;
			break;
		/*SEEK_CUR 新的读写位置是相对于当前的位置再增加offset个位移量*/
		case SEEK_CUR:
			new_pos = (int32_t)pf->fd_pos + offset;
			break;
		/*SEEK_END 新的读写位置是相对于文件尺寸再增加offset个位移量*/
		case SEEK_END:
			new_pos = file_size + offset;
	}
	if(new_pos < 0 || new_pos > (file_size - 1)){
		return -1;
	}
	pf->fd_pos = new_pos;
	return pf->fd_pos;
}

/*删除文件，成功返回0，失败返回-1*/
int32_t sys_ulink(const char *pathname){
	ASSERT(strlen(pathname) < MAX_PATH_LEN );
	/*先检查待删除的文件是否存在*/
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = search_file(pathname, &searched_record);
	ASSERT(inode_no != 0);
	if(inode_no == -1){
		printk("file %s not found\n", pathname);
		dir_close(searched_record.parent_dir);
		return -1;	
	}
	if(searched_record.file_type == FT_DIRECTORY){
		printk("can't delete a directory with ulink(), use rmdir() to instead\n");
		dir_close(searched_record.parent_dir);
		return -1;
	}

	/*检查是否在已打开的文件列表中*/
	uint32_t file_idx = 0;
	while(file_idx < MAX_FILE_OPEN){
		if(file_table[file_idx].fd_inode != NULL &&\
			(uint32_t)inode_no == file_table[file_idx].fd_inode->i_no) break;
		++file_idx;
	}
	if(file_idx < MAX_FILE_OPEN){
		dir_close(searched_record.parent_dir);
		printk("file %s is in use, not allow to delete!\n", pathname);
		return -1;
	}
	ASSERT(file_idx == MAX_FILE_OPEN);
	/*为delete_dir_entry申请缓冲区*/
	void *io_buf = sys_malloc(SECTOR_SIZE  + SECTOR_SIZE);
	if(io_buf == NULL){
		dir_close(searched_record.parent_dir);
		printk("sys_ulink: malloc for io_buf failed\n");
		return -1;
	}
	struct dir *parent_dir = searched_record.parent_dir;
	delete_dir_entry(cur_part, parent_dir, inode_no, io_buf);
	inode_release(cur_part, inode_no);
	 
	sys_free(io_buf);	
	dir_close(searched_record.parent_dir);
	return 0;  //成功删除文件
}

/*创建目录pathname，成功返回0，失败返回-1*/
int32_t sys_mkdir(const char *pathname){
	uint8_t rollback_step = 0; //用于操作失败时回滚各资源的状态
	void *io_buf = sys_malloc(SECTOR_SIZE * 2);
	if(io_buf == NULL){
		printk("sys_mkdir: sys malloc for io_buf failed\n");
		return -1;
	}

	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = -1;
	inode_no = search_file(pathname, &searched_record);
	if(inode_no != -1){ //找到同名的目录和文件，失败则返回
		printk("sys_mkdir: file or directory %s exist in!\n", pathname);
		rollback_step = 1;
		goto rollback;
	}else{//若未找到，也要判断是在最终目录没找到，还是中间目录没找到
		printk("sys_mkdir: searched_record: %s\n", searched_record.searched_path);
		uint32_t pathname_depth = path_depth_cnt((char *)pathname);
		uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
		/*先判断是否把pathname的各层目录都访问到了，还是访问中间目录就失败了*/
		if(pathname_depth != path_searched_depth){
			//说明并没有访问到全部的路径，某个中间目录不存在
			printk("sys_mkdir: cannot access %s: Not a directory, subpath %s isn't exist\n", pathname, searched_record.searched_path);
			rollback_step = 1;
			goto rollback;
		}
	}

	struct dir *parent_dir = searched_record.parent_dir;
	/*目录名称后可能会有字符'/', 所以最好直接用searched_reocrd.searched_path*/
	char *dirname = strrchr(searched_record.searched_path, '/') + 1; 
	inode_no = inode_bitmap_alloc(cur_part);
	if(inode_no == -1){
		printk("sys_mkdir: allocate inode failed\n");
		rollback_step = 1;
		goto rollback;
	}

	struct inode new_dir_inode;
	inode_init(inode_no, &new_dir_inode); //初始化i结点	

	uint32_t block_bitmap_idx = 0;
	//用来记录block对应于block_bitmap中的索引
	int32_t block_lba = -1;

	/*为目录分配一个块，用来写入目录.和..*/
	block_lba = block_bitmap_alloc(cur_part);
	if(block_lba == -1){
		printk("sys_mkdir: block_bitmap alloc for create directory failed");
		rollback_step = 2;
		goto rollback;
	}
	new_dir_inode.i_sectors[0] = block_lba;
	/*每分配一个块就将位图同步到硬盘`o*/
	block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
	ASSERT(block_bitmap_idx != 0);
	bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

	/*将当前目录的目录项.和..写入目录*/
	memset(io_buf, 0, SECTOR_SIZE *2);
	struct dir_entry *p_de = (struct dir_entry *)io_buf;
	/*初始化当前目录"."*/
	memcpy(p_de->filename, ".", 1);
	p_de->i_no = inode_no;
	p_de->f_type = FT_DIRECTORY;

	++p_de;
	/*初始化当前目录".."*/
	memcpy(p_de->filename, "..", 2);
	p_de->i_no = parent_dir->inode->i_no;
	p_de->f_type = FT_DIRECTORY;
	ide_write(cur_part->my_disk, new_dir_inode.i_sectors[0], io_buf, 1);

	new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;

	/*在父目录中添加自己的目录项*/
	struct dir_entry new_dir_entry;
	memset(&new_dir_entry, 0, sizeof(struct dir_entry));
	create_dir_entry(dirname, inode_no, FT_DIRECTORY, &new_dir_entry);
	memset(io_buf, 0, SECTOR_SIZE * 2); //清空io_buf

	if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)){
		//sync_dir_entry中将block_bitmap通过bitmap_sync同步到硬盘
		printk("sys_mkdir: sync_dir_entry to disk failed\n");
		rollback_step = 2;
		goto rollback;
	}
	/*父目录的inode同步到硬盘*/
	memset(io_buf, 0, SECTOR_SIZE * 2);
	inode_sync(cur_part, parent_dir->inode, io_buf);

	/*将新创建目录的inode同步到硬盘*/
	memset(io_buf, 0, SECTOR_SIZE * 2);
	inode_sync(cur_part, &new_dir_inode, io_buf);

	/*将inode位图同步到硬盘*/
	bitmap_sync(cur_part, inode_no, INODE_BITMAP);

	sys_free(io_buf);
	/*关闭创建目录的父目录*/
	dir_close(searched_record.parent_dir);
	return 0;
	/*创建文件或者目录需要创建多个资源，若某步失效则会执行到下面的回滚步骤*/
rollback:
	switch(rollback_step){
		case 2:
			bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
			//如果新文件的inode创建失败，之前位图中分配的inode_no也要恢复
		case 1:
			/*关闭所创建目录的父目录*/
			dir_close(searched_record.parent_dir);
			break;
	}
	sys_free(io_buf);
	return -1;
}

/*目录打开成功后返回目录指针，失败返回NULL*/
struct dir *sys_opendir(const char* name){
	ASSERT(strlen(name) < MAX_PATH_LEN);
	/*如果是根目录'/',直接返回&root_dir*/
	if(name[0] == '/' &&(name[1] == 0 || name[1] == '.')) {
		return  &root_dir;
	}
	/*先检查待打开的目录是否存在*/
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = search_file(name, &searched_record);
	struct dir *ret = NULL;
	if(inode_no == -1){
		printk("In %s, sub path %s not exist\n", name, searched_record.searched_path);
	}else{
		if(searched_record.file_type == FT_REGULAR){
			printk("%s is regular file\n", name);
		}else if(searched_record.file_type == FT_DIRECTORY){
			ret = dir_open(cur_part, inode_no);
		}
	}
	dir_close(searched_record.parent_dir);
	return ret;
}

/*成功关闭目录p_dir返回0， 失败返回-1*/
int32_t sys_closedir(struct dir * dir){
	int32_t ret = -1;
	if(dir != NULL){
		dir_close(dir);
		ret = 0;
	}
	return ret;
}

/*读取目录dir的1个目录项，成功后返回其目录项地址，到目录尾或出错时返回NULL*/
struct dir_entry* sys_readdir(struct dir *dir){
	ASSERT(dir != NULL);
	return dir_read(dir);
}

/*把目录dir的指针dir_pos置0*/
void sys_rewinaddr(struct dir * dir){
	dir->dir_pos = 0;
}

/*删除空目录，成功时返回0，失败时返回-1*/
int32_t sys_rmdir(const char *pathname){
	/*先检查待删除文件是否存在*/
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = search_file(pathname, &searched_record);
	ASSERT(inode_no != 0);
	int retval = -1; //默认返回值
	if(inode_no == -1){
		printk("In %s, sub path %s not exist\n", pathname, searched_record.searched_path);
	}else{
		if(searched_record.file_type == FT_REGULAR){
			printk("%s is regular file\n", pathname);
		}else{
			struct dir *dir = dir_open(cur_part, inode_no);
			if(!dir_is_empty(dir)){ //非空目录不可删除
				printk("dir %s is not empty, it is not allowed to delete a nonempty directory\n", pathname);
			}else{
				if(!dir_remove(searched_record.parent_dir, dir)){
					retval = 0;
				}
			}
			dir_close(dir);
		}
	}
	dir_close(searched_record.parent_dir);
	return retval;
}

/*获取父目录的inode编号*/
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr, void *io_buf){
	struct inode *child_dir_inode =  inode_open(cur_part, child_inode_nr);
	/*目录中的目录项".."包括父目录的inode编号,".."位于目录的第0块*/
	uint32_t block_lba = child_dir_inode->i_sectors[0];
	ASSERT(block_lba >= cur_part->sb->data_start_lba);
	inode_close(child_dir_inode);
	ide_read(cur_part->my_disk, block_lba, io_buf, 1);
	struct dir_entry *dir_e = (struct dir_entry *)io_buf;
	/*第0个目录项是.,第1个目录项是..*/
	ASSERT(dir_e[1].i_no < 4096 && dir_e[1].f_type == FT_DIRECTORY);
	return dir_e[1].i_no; //返回父目录的inode编号
}

/*在inode编号为p_inode_nr的目录中查找，
 * inode编号为c_inode_nr的子目录的名字
 * 将名字存入缓冲区path, 成功返回0，失败返回-1*/
static int get_child_dir_name(uint32_t p_inode_nr, uint32_t c_inode_nr, char *path, void *io_buf){
	struct inode *parent_dir_inode = inode_open(cur_part, p_inode_nr);
	/*填充all_blocks，将该目录所占扇区地址全部写入all_blocks*/
	uint8_t block_idx = 0;
	uint32_t *all_blocks = (uint32_t *)sys_malloc(SECTOR_SIZE + 48);
	uint32_t block_cnt = 12;
	while(block_idx < 12){
		all_blocks[block_idx] = parent_dir_inode->i_sectors[block_idx];
		++block_idx;
	}
	if(parent_dir_inode->i_sectors[12]){
		//若包含一级间接块表，将其读入all_blocks
		ide_read(cur_part->my_disk, parent_dir_inode->i_sectors[12], all_blocks + 12, 1);
		block_cnt = 140;
	}
	inode_close(parent_dir_inode);

	struct dir_entry *dir_e = (struct dir_entry *) io_buf;
	uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
	uint32_t dir_entrys_per_sec = (512 / dir_entry_size);
	block_idx = 0;
	/*遍历所有块*/
	while(block_idx < block_cnt){
		if(all_blocks[block_idx]){
			ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
			uint8_t dir_e_idx = 0;
			/*遍历每个目录项*/
			while(dir_e_idx < dir_entrys_per_sec){
				if((dir_e + dir_e_idx)->i_no == c_inode_nr){
					strcat(path, "/");
					strcat(path, (dir_e + dir_e_idx)->filename);
					return 0;
				}
				++dir_e_idx;
			}
		}
		++block_idx;
	}
	return -1;
}

/*把当前工作目录绝对路径写入buf,size是buf大小
 * 当buf为NULL时，由操作系统分配存储工作路径的空间并返回地址，
 * 失败则返回NULL*/
char *sys_getcwd(char *buf, uint32_t size){
	/*确保buf不为空，若用户进程提供的buf为NULL,
	 * 系统调用getcwd中要为用户进程用过malloc分配*/
	ASSERT(buf != NULL);
	void *io_buf = sys_malloc(SECTOR_SIZE);
	if(io_buf == NULL){
		return NULL;
	}

	struct task_struct* cur_thread = running_thread();
	int32_t parent_inode_nr= 0;
	int32_t child_inode_nr = cur_thread->cwd_inode_nr;
	ASSERT(child_inode_nr >= 0 && child_inode_nr < 4096);
	//最大支持4096个inode
	/*若当前目录是根目录，直接返回'/'*/
	if(child_inode_nr == 0){
		buf[0] = '/';
		buf[1] = 0;
		return buf;
	}
	memset(buf, 0, size);
	char full_path_reverse[MAX_PATH_LEN] = {0}; //用来做全路径缓冲区
	/*从下往上逐层找父目录，直到找到根目录为止
	 * 当child_inode_nr为根目录的inode编号{0}时停止
	 * 即已经查看玩根目录的目录项*/
	while((child_inode_nr)){
		parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr, io_buf);
		if(get_child_dir_name(parent_inode_nr, child_inode_nr, \
					full_path_reverse, io_buf) == -1){
			sys_free(io_buf);
			return NULL;
		}
		child_inode_nr = parent_inode_nr;
	}
	ASSERT(strlen(full_path_reverse) <= size);
	/*至此full_path_reverse中的路径是反着的
	 * 即子目录在前，父目录灾后
	 * 现将full_path_reverse中的路径反置*/
	char *last_slash; //用于记录字符串中最后一个斜杠地址
	while( (last_slash = strchr(full_path_reverse, '/')) ){
		uint16_t len = strlen(buf);
		strcpy(buf + len, last_slash);
		/*在full_path_reverse中添加结束字符
		 * 作为下一次执行strcpy中last_slash的边界*/
		*last_slash = 0;
	}
	sys_free(io_buf);
	return buf;
}

/*更改当前目录为绝对路径path,成功则返回0，失败则返回-1*/
int32_t sys_chdir(const char*path){
	int32_t ret = -1;
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	int inode_no = search_file(path, &searched_record);
	if(inode_no != -1){
		if(searched_record.file_type == FT_DIRECTORY){
			running_thread()->cwd_inode_nr = inode_no;
			ret = 0;
		}else{
			printk("sys_chdir: %s is regular file or other!\n", path);
		}
	}
	dir_close(searched_record.parent_dir);
	return ret;
}

/*在buf中填文件结构的相关信息，成功时返回0，失败返回-1*/
int32_t sys_stat(const char *path, struct stat *buf){
	/*若直接查看根目录'/'*/
	if(strcmp(path, "/") == 0 || strcmp(path, "/.") == 0 ||strcmp(path, "/..") == 0 ){
		buf->st_filetype = FT_DIRECTORY;
		buf->st_ino = 0;
		buf->st_size = root_dir.inode->i_size;
		return 0;
	}

	int32_t ret = -1;
	struct path_search_record searched_record;
	memset(&searched_record, 0, sizeof(struct path_search_record));
	//记得初始化或清0， 否则栈中信息
	int inode_no = search_file(path, &searched_record);
	if(inode_no != -1){
		struct inode *obj_inode = inode_open(cur_part, inode_no);
		//只为获得文件大小
		buf->st_size = obj_inode->i_size;
		inode_close(obj_inode);
		buf->st_filetype = searched_record.file_type;
		buf->st_ino = inode_no;
		ret = 0;
	}else{
		printk("sys_stat: %s not found\n", path);
	}
	dir_close(searched_record.parent_dir);
	return ret;
}

void filesys_init2(){
	char default_part[8] = "sdb1";
	/*挂载分区*/
	list_traversal(&partition_list, mount_partition, (int)default_part);

	/*将当前分区的根目录打开*/
	open_root_dir(cur_part);

	/*初始化文件表*/
	uint32_t fd_idx = 0;
	while(fd_idx < MAX_FILE_OPEN){
		file_table[fd_idx++].fd_inode = NULL;
	}
}
