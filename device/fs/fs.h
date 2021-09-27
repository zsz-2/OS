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

/*关于路径解析*/
#define MAX_PATH_LEN 512

/*文件类型*/
enum oflags{
	O_RDONLY, //只读
	O_WRONLY, //只写
	O_RDWR,  //读写
	O_CREAT = 4 //创建
};

/*用来记录查找文件过程中的上级路径，也就是查找文件过程中经过的地方*/
struct path_search_record{
	char searched_path[MAX_PATH_LEN];
	struct dir *parent_dir; //文件或目录所在的直接父目录
	enum file_types file_type; //找到的是普通文件，还是目录，找不到的将是未知类型
};


struct partition *cur_part; //默认情况下操作的是哪个分区


#endif
