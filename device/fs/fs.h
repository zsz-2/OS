#ifndef __FS__H
#define __FS__H

#define MAX_FILES_PER_PART 4096 //每个分区支持最大创建文件数
#define BITS_PER_SECTOR 4096  //每扇区的位数
#define SECTOR_SIZE 512 //扇区的字节大小
#define BLOCK_SIZE SECTOR_SIZE //块的字节大小

/*文件类型*/
enum file_types{
	FT_UNKNOWN,//不支持的文件类型
	FT_REGULAR, //普通文件
	FT_DIRECTORY //块字节大小
};


void filesys_init();
#endif
