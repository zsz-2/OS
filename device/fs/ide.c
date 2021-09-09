#include "ide.h"
#include "fs.h"
#include "interrupt.h"
#include "timer.h"
#include "stdio-kernel.h"
#include "io.h"
#include "global.h"
#include "list.h"
#include "stdint.h"
#include "bitmap.h"
#include "sync.h"

/*定义硬盘中各寄存器的端口号*/
#define reg_data(channel)  (channel->port_base + 0)
#define reg_error(channel)  (channel->port_base + 1)
#define reg_sec_cnt(channel)  (channel->port_base + 2)
#define reg_lba_l(channel)  (channel->port_base + 3)
#define reg_lba_m(channel)  (channel->port_base + 4)
#define reg_lba_h(channel)  (channel->port_base + 5)
#define reg_dev(channel)  (channel->port_base + 6)
#define reg_status(channel)  (channel->port_base + 7)
#define reg_cmd(channel)  (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)  (reg_alt_status(channel))

/*reg_alt_status的一些关键位*/
#define BIT_STAT_BSY 0x80
#define BIT_STAT_DRDY  0x40 //驱动器准备好
#define BIT_STAT_DRQ 0x08 //数据传输准备好了

/*device寄存器一些关键位*/
#define BIT_DEV_MBS 0xa0
#define BIT_DEV_LBA 0x40
#define BIT_DEV_DEV 0x10

/*一些硬盘操作指令*/
#define CMD_IDENTIFY 0xec   //识别硬盘指令
#define CMD_READ_SECTOR 0x20 //读扇区指令
#define CMD_WRITE_SECTOR 0x30 //写扇区指令

/*最大可写的扇区数，只针对80MB*/
#define max_lba (80 * 1024 * 1024)/512 -1 


/*选择读写的硬盘*/
static void select_disk(struct disk *hd){
	uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
	if(hd->dev_no == 1){
		reg_device |= BIT_DEV_DEV;
	}
	outb(reg_dev(hd->my_channel), reg_device);
}

/*向硬盘控制器写入起始扇区地址及要读写的扇区数目*/
static void select_sector(struct disk*hd, uint32_t lba, uint8_t sec_cnt){
	ASSERT(lba < max_lba);
	struct ide_channel *channel = hd->my_channel;
	/*写入要写的扇区数*/	
	//sector_count寄存器是8位宽度，范围是0~255，当寄存器为0时，表示256个扇区
	outb(reg_sec_cnt(channel), sec_cnt); //若sec_cnt为0，表示写入256个扇区	

	/*写入lba地址*/
	outb(reg_lba_l(channel), lba);
	outb(reg_lba_m(channel), lba >> 8);
	outb(reg_lba_h(channel), lba >> 16);

	/*写入device寄存器，为的是输入LBA地址第24~27位*/
	outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? \
				BIT_DEV_DEV : 0)  | (lba >> 24));
}


/*向channel发命令cmd*/
static void cmd_out(struct ide_channel *channel, uint8_t cmd){
	/*只要硬盘发出了命令，并将此标记设为true*/
	channel->expecting_intr = true;
	outb(reg_cmd(channel), cmd);
}

/*硬盘读入sec_cnt个扇区到buf*/
static void read_from_sector(struct disk *hd, void *buf, uint8_t sec_cnt){
	uint32_t size_in_byte;
	if(sec_cnt == 0){
		size_in_byte = 256 * 512;
	}else{
		size_in_byte = sec_cnt * 512;
	}
	insw(reg_data(hd->my_channel), buf, size_in_byte/2);
}


/*将buf写入sec_cnt个扇区*/
static void write2sector(struct disk *hd, void *buf, uint8_t sec_cnt){
	uint32_t size_in_byte;
	if(sec_cnt == 0){
		size_in_byte = 256 * 12;
	}else{
		size_in_byte = sec_cnt * 512;
	}
	outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}


/*等待30秒*/
static bool busy_wait(struct disk *hd){
	struct ide_channel *channel = hd->my_channel;
	uint16_t time_limit = 30 * 1000;
	while(time_limit -= 10 >= 0){
		if(!(inb(reg_status(channel)) &  BIT_STAT_BSY )){
			return (inb(reg_status(channel)) & BIT_STAT_DRQ);
		}else{
			mtime_sleep(10);
		}
	}	
}

/*从硬盘读取sec_cnt个扇区到buf*/
void ide_read(struct disk *hd, uint32_t lba, void *buf, uint32_t sec_cnt){
	ASSERT(lba < max_lba);
	ASSERT(sec_cnt > 0);
	lock_acquire(&hd->my_channel->lock);
	
	/*选择操作的硬盘*/
	select_disk(hd);

	uint32_t secs_op;   //每次操作的扇区数
	uint32_t secs_done = 0; //已完成的扇区数

	while(secs_done < sec_cnt){
		if((secs_done + 256) <= sec_cnt){
			secs_op = 256;
		}else{
			secs_op = sec_cnt - secs_done;
		}
		/*写入待读入的扇区和起始扇区号*/
		select_sector(hd, lba + secs_done, secs_op);

		/*执行的命令写入reg_cmd寄存器*/
		cmd_out(hd->my_channel, CMD_READ_SECTOR);

		/*硬盘开始工作后，将自己阻塞，等待处理完成后，唤醒自己*/
		sema_down(&hd->my_channel->disk_done);

		/*检测硬盘状态是否可读*/
		if(!busy_wait(hd)){
			char error[64];
			sprintf(error, "%s read sector %d failed!!!!!\n", hd->name, lba);
			PANIC(error);
		}

		read_from_sector(hd,(void *)((uint32_t)buf + secs_done * 512), secs_op);
		secs_done += secs_op;
	}
	lock_release(&hd->my_channel->lock);
}

/*将buf中sec_cnt扇区写入硬盘*/
void ide_write(struct disk *hd, uint32_t lba, void *buf, uint32_t sec_cnt){
	ASSERT(lba < max_lba);
	ASSERT(sec_cnt > 0);
	lock_acquire(&hd->my_channel->lock);
	/*1.选择操作的硬盘*/
	select_disk(hd);

	uint32_t secs_op;
	uint32_t secs_done = 0;
	while(secs_done < sec_cnt){
		if((secs_done + 256) <= sec_cnt){
			secs_op = 256;
		}else{
			secs_op = sec_cnt - secs_done;
		}
		
		/*2.写入待写入的扇区和起始扇区号*/
		select_sector(hd, lba + secs_done, secs_op);

		/*3.执行的命令写入reg_cmd寄存器*/
		cmd_out(hd->my_channel, CMD_WRITE_SECTOR);

		/*4.检测硬盘状态是否可读*/
		if(!busy_wait(hd)){
			char error[64];
			sprintf(error,"%s write sector %d failed!!!!!\n", hd->name, lba);
			PANIC(error);
		}
		
		/*5.将数据写入硬盘*/
		write2sector(hd, (void *)((uint32_t)buf + secs_done * 512), secs_op);

		/*在硬盘响应期间堵塞自己*/
		sema_down(&hd->my_channel->disk_done);
		secs_done += secs_op;
	}
	/*醒来之后释放锁*/
	lock_release(&hd->my_channel->lock);
}

/*硬盘中断处理程序*/
void intr_hd_handler(uint8_t irq_no){
	ASSERT(irq_no == 0x2e || irq_no == 0x2f);
	uint8_t ch_no = irq_no - 0x2e;
	struct ide_channel *channel = &channels[ch_no];
	ASSERT(channel->irq_no == irq_no);
	/*不用担心此中断是否对应这一次的expecting_intr
	 * 每次读写硬盘时会申请锁，从而保证同步一致性*/
	if(channel->expecting_intr){
		channel->expecting_intr = false;
		sema_up(&channel->disk_done);
	}
	/*读取状态寄存器让硬盘控制器认为该中断已经被处理，可以继续执行新的读写*/
	inb(reg_status(channel));
}


/*用于记录总扩展分区的起始lba,初始为0*/
int32_t ext_lba_base = 0; 

/*记录硬盘主分区和逻辑分区的下标*/
uint8_t p_no = 0, l_no = 0; 


/*用来存分区表项*/
struct partition_table_entry{
	uint8_t bootable; //是否可引导
	uint8_t start_head; //起始磁头号
	uint8_t start_sec; //起始扇区号
	uint8_t start_chs;  //起始柱面号
	uint8_t fs_type; //分区类型
	uint8_t end_head; //结束磁头号
	uint8_t end_sec; //结束扇区号
	uint8_t end_chs; //结束柱面号
	uint32_t start_lba; //本分区起始扇区的lba地址
	uint32_t sec_cnt; //本分区的扇区数目
}__attribute__((packed));


/*引导扇区,mbr或者ebr所在的扇区*/
struct boot_sector{
	uint8_t other[446];
	struct partition_table_entry partition_table[4];
	uint16_t signature; //启动扇区的结束标志是0x55, 0xaa
}__attribute__((packed));


/*将dst中len个相邻字节交换位置后存入buf*/
static void swap_pairs_byte(const char *dst, char *buf, uint32_t len){
	uint8_t idx;
	for(idx = 0; idx < len; idx += 2){
		buf[idx + 1] = *dst++;
		buf[idx] = *dst++;
	}
	buf[idx] = '\0';
}

/*获得硬盘参数信息*/
static identify_disk(struct disk* hd){
	char id_info[512];
	select_disk(hd);
	
	cmd_out(hd->my_channel, CMD_IDENTIFY);

	/*硬盘发送完指令后阻塞自己，等到中断处理程序将自己唤醒*/
	sema_down(&hd->my_channel->disk_done);

	if(!busy_wait(hd)){
		char error[64];
		sprintf(error, "%s identify failed!!!!!!\n", error);
		PANIC(error);
	}
	
	read_from_sector(hd, id_info, 1);

	char buf[64];
	uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;

	swap_pairs_byte(&id_info[sn_start], buf, sn_len);
	printk(" disk %s info:\n    SN: %s\n", hd->name, buf);
	memset(buf, 0, sizeof(buf));

	swap_pairs_byte(&id_info[md_start], buf, md_len);
	printk("   MOUDLE:%s\n",buf);
	uint32_t sectors = *(uint32_t *)&id_info[60 * 2];
	printk("    SECTORS: %d\n",sectors);
	printk("    CAPACITY:%d\n",sectors * 512 / 1024/1024);
}

/*扫描硬盘hd中地址位ext_lba的所有分区*/
static void partition_scan(struct disk *hd, uint32_t ext_lba){
	//printk("-----------------     %d\n", sizeof(struct boot_sector));
	struct boot_sector *bs = sys_malloc(sizeof(struct boot_sector));
	ide_read(hd, ext_lba, bs, 1);
	uint8_t part_idx = 0;
	struct partition_table_entry *p =  bs->partition_table;

	/*遍历分区表4个表项*/
	while(part_idx++ < 4){
		if(p->fs_type == 0x05){
			if(ext_lba_base != 0){
				/*子扩展分区的start_lba是相对于 主引导扇区的总扩展分区地址*/
				partition_scan(hd, p->start_lba + ext_lba_base);
			}else{
				/*记录扩展分区的lba地址*/
				ext_lba_base = p->start_lba;
				partition_scan(hd,p->start_lba);
			}
		}else if(p->fs_type != 0){
			if(ext_lba == 0){
				//此时为主分区
				hd->prim_parts[p_no].start_lba =  ext_lba + p->start_lba;
				hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
				hd->prim_parts[p_no].my_disk = hd;
				list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
				sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no + 1);
				++p_no;
				ASSERT(p_no < 4);
			}else{
				hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
				hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
				hd->logic_parts[l_no].my_disk = hd;
				list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
				sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name, l_no + 5);
				++l_no;
				if(l_no >= 8) return;
			}
		}
		++p;
	}
	sys_free(bs);
}

static bool partition_info(struct list_elem *pelem, int arg UNUSED){
	struct partition *part = elem2entry(struct partition, part_tag, pelem);
	printk("%s start_lba: %d, sec_cnt: %d\n",\
			part->name, part->start_lba, part->sec_cnt);
	//返回false为了能使用list_traversal函数
	return false;
}




/*硬盘数据结构初始化*/
void ide_init(){
	printk("ide_init start\n");
	list_init(&partition_list);
	uint8_t hd_cnt = *((uint8_t*)(0x475));
	ASSERT(hd_cnt > 0);
	channel_cnt = DIV_ROUND_UP(hd_cnt, 2); //根据硬盘数量反推有几个ide通道

	struct ide_channel *channel;
	uint8_t channel_no = 0;
	uint8_t dev_no = 0;
	
	printk("channle_cnt: %d\n",channel_cnt);

	while(channel_no < channel_cnt){
		printk("---------------------------------\n");
		channel = &channels[channel_no];
		sprintf(channel->name, "ide%d", channel_no);
		switch (channel_no){
			case 0:
				channel->port_base = 0x1f0;
				channel->irq_no = 0x20 + 14;
				break;
			case 1:
				channel->port_base = 0x170;
				channel->irq_no = 0x20 + 15;
				break;
		}
		channel->expecting_intr = false; //未向硬盘写入指令时不期待硬盘的中断
		lock_init(&channel->lock);
		/*设置信号量为0，目的是向硬盘控制器请求数据后，硬盘驱动sema_down会阻塞程序i
		 * 知道硬盘完成后发送中断，由中断处理程序sema_up，唤醒线程*/
		sema_init(&channel->disk_done, 0);
		register_handler(channel->irq_no, intr_hd_handler);
		while(dev_no < 2){
			struct disk *hd = &channel->devices[dev_no];
			hd->my_channel = channel;
			hd->dev_no = dev_no;
			sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
			identify_disk(hd);
			if(dev_no != 0){
				partition_scan(hd, 0); //扫描硬盘上的分区
			}
			p_no = 0; l_no = 0;
			++dev_no;
		}
		dev_no = 0;
		++channel_no;
	}
	printk("\n all partition info\n");
	list_traversal(&partition_list, partition_info, (int)NULL);
	printk("ide_init done\n");

}


