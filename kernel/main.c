#include "print.h"
#include "fs.h"
#include "userprog.h"
#include "keyboard.h"
#include "interrupt.h"
#include "debug.h"
#include "init.h"
#include "thread.h"
#include "syscall.h"
#include "stdio.h"
#include "ide.h"
#include "inode.h"
#include "super_block.h"
#include "dir.h"

/*
void consumer_a(void *);
void consumer_b(void *);
*/

void k_thread_a(void *);
void k_thread_b(void *);
void u_prog_a(void *);
void u_prog_b(void *);
int prog_a_pid = 0, prog_b_pid = 0;

int main(void){
	init_all();
	/*
	process_execute(u_prog_a, "user_prog_a");

	process_execute(u_prog_b, "user_prog_b");
	//intr_enable();

	thread_start("k_thread_a", 31, k_thread_a, "A_ ");
	thread_start("k_thread_b", 31, k_thread_b, "B_ ");
	
	
	intr_enable();
	*/

	//sys_open("/file1", O_CREAT);

	/*
	uint32_t fd = sys_open("/file1", O_RDWR);
	printf("fd:%d\n",fd);
	sys_write(fd,"hello,world\n", 12);
	sys_close(fd);
	printk("%d closed now\n", fd);
	*/
	
	/*
	uint32_t fd = sys_open("/file1", O_RDWR);
	printk("open /file1, fd: %d\n", fd);
	char buf[64] = {0};
	int read_bytes = sys_read(fd, buf, 100);
	printf("1_read %d bytes:\n%s\n", read_bytes, buf + 25);
	sys_lseek(fd, 0, SEEK_SET);
	read_bytes = sys_read(fd, buf, 100);
	printf("1_read %d bytes:\n%s\n", read_bytes, buf + 25);
	*/
	//sys_open("/file1", O_CREAT);
	//sys_mkdir("/dir1/subdir1");

	
	//printk("/file1 delete %s!\n", ( (sys_ulink("/file1") == 0) ? : "done" , "fail"));
	//printk("/file1 delete %d!\n", sys_ulink("/dir1/subdir1/file2") );
	
	//int fd = sys_open("/dir1/subdir1/file2", O_CREAT | O_RDWR);
	//sys_write(fd, "Catch me if you can\n", 21);

	/*
	struct dir *p_dir = sys_opendir("/dir1/subdir1");
	if(p_dir){
		printf("/dir1/subdir1 open done\n");
		if(sys_closedir(p_dir) == 0){
			printk("/dir1/subdir1 close done\n");
		}else{
			printk("/dir1/subdir1 close fail\n");
			
		}
	}else{
		printk("/dir1/subdir1 open fail\n");
	}
	*/
	struct dir *p_dir = sys_opendir("/dir1");
	if(p_dir){
		printk("/dir1/subdir1 open done!\ncontent:\n");
		char *type = NULL;
		struct dir_entry *dir_e = NULL;
		while((dir_e = sys_readdir(p_dir))){
			if(dir_e->f_type == FT_REGULAR){
				type = "regular";
			}else{
				type = "directory";
			}
			printk("    %s    %s\n", type, dir_e->filename);
		}
		if(sys_closedir(p_dir) == 0){
			printk("/dir1/subdir1 close done\n");
		}else{
			printk("/dir1/subdir1 close fail\n");
		}
	}else{
		printk("/dir1/subdir1 open faile\n");
	}
	printk("%d\n", cur_part->sb->block_bitmap_lba);
	printk("%d\n", cur_part->sb->inode_bitmap_lba);
	printk("%d\n", cur_part->sb->inode_table_lba);
	printk("%d\n", cur_part->sb->data_start_lba);
	while(1){}
	return 0;
}

void k_thread_a(void *arg){
	void *addr1 = sys_malloc(256);
	void *addr2 = sys_malloc(255);
	void *addr3 = sys_malloc(254);
	console_put_str("thread_a malloc addr:0x");
	console_put_int((int)addr1);
	console_put_char('\n');
	console_put_int((int)addr2);
	console_put_char('\n');
	console_put_int((int)addr3);
	console_put_char('\n');

	int cpu_delay = 100000;
	while(cpu_delay-- > 0);
	
	sys_free(addr1);
	sys_free(addr2);
	sys_free(addr3);
	while(1);
}

void k_thread_b(void *arg){
	void *addr1 = sys_malloc(256);
	void *addr2 = sys_malloc(255);
	void *addr3 = sys_malloc(254);
	console_put_str("thread_a malloc addr:0x");
	console_put_int((int)addr1);
	console_put_char('\n');
	console_put_int((int)addr2);
	console_put_char('\n');
	console_put_int((int)addr3);
	console_put_char('\n');

	int cpu_delay = 100000;
	while(cpu_delay-- > 0);
	
	sys_free(addr1);
	sys_free(addr2);
	sys_free(addr3);
	while(1);
}


void u_prog_a(void *arg){
	void *addr1 = malloc(256);
	void *addr2 = malloc(255);
	void *addr3 = malloc(254);
	printf(" prog_a malloc addr: 0x%x, 0x%x, 0x%x\n", (int)addr1, (int)addr2, (int)addr3);
	

	int cpu_delay = 100;
	while(cpu_delay-- > 0);
	
	free(addr1);
	free(addr2);
	free(addr3);
	while(1);
}

void u_prog_b(void *arg){
	void *addr1 = malloc(256);
	void *addr2 = malloc(255);
	void *addr3 = malloc(254);
	printf(" prog_b malloc addr: 0x%x, 0x%x, 0x%x\n", (int)addr1, (int)addr2, (int)addr3);
	

	int cpu_delay = 100;
	while(cpu_delay-- > 0);
	
	free(addr1);
	free(addr2);
	free(addr3);
	while(1);
}
