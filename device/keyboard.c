#include "keyboard.h"
#include "ioqueue.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"

#define KBD_BUF_PORT 0x60  //键盘buffer寄存器端口号为0x60


/*用转义字符定义各种控制字符*/
#define esc '\033'
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\177'

#define char_invisible 0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible


/*定义控制字符的通码和断码*/
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a

/*定义以下变量是否处于按下状态*/
static int ctrl_status, shift_status, alt_status, caps_lock_status, ext_scancode;


/*以makecode为索引的二维数组*/
static char key_map[][2] = {
/*0x00*/  {0, 0},
/*0x01*/  {esc, esc},
/*0x02*/  {'1', '!'},
/*0x03*/  {'2', '@'},
/*0x04*/  {'3', '#'},
/*0x05*/  {'4', '$'},
/*0x06*/  {'5', '%'},
/*0x07*/  {'6', '^'},
/*0x08*/  {'7', '&'},
/*0x09*/  {'8', '*'},
/*0x0a*/  {'9', '('},
/*0x0b*/  {'0', ')'},
/*0x0c*/  {'-', '_'},
/*0x0D*/  {'=', '+'},
/*0x0E*/  {backspace, backspace},
/*0x0F*/  {tab, tab},
/*0x10*/  {'q', 'Q'},
/*0x11*/  {'w', 'W'},
/*0x12*/  {'e', 'E'},
/*0x13*/  {'r', 'R'},
/*0x14*/  {'t', 'T'},
/*0x15*/  {'y', 'Y'},
/*0x16*/  {'u', 'U'},
/*0x17*/  {'i', 'I'},
/*0x18*/  {'o', 'O'},
/*0x19*/  {'p', 'P'},
/*0x1A*/  {'[', '{'},
/*0x1B*/  {']', '}'},
/*0x1C*/  {enter, enter},
/*0x1D*/  {ctrl_l_char, ctrl_l_char},
/*0x1E*/  {'a', 'A'},
/*0x1F*/  {'s', 'S'},
/*0x20*/  {'d', 'D'},
/*0x21*/  {'f', 'F'},
/*0x22*/  {'g', 'G'},
/*0x23*/  {'h', 'H'},
/*0x24*/  {'j', 'J'},
/*0x25*/  {'k', 'K'},
/*0x26*/  {'l', 'L'},
/*0x27*/  {';', ':'},
/*0x28*/  {'\'', '"'},
/*0x29*/  {'`', '~'},
/*0x2A*/  {shift_l_char, shift_l_char},
/*0x2B*/  {'\\', '|'},
/*0x2C*/  {'z', 'Z'},
/*0x2D*/  {'x', 'X'},
/*0x2E*/  {'c', 'C'},
/*0x2F*/  {'v', 'V'},
/*0x30*/  {'b', 'B'},
/*0x31*/  {'n', 'N'},
/*0x32*/  {'m', 'M'},
/*0x33*/  {',', '<'},
/*0x34*/  {'.', '>'},
/*0x35*/  {'/', '?'},
/*0x36*/  {shift_r_char, shift_r_char},
/*0x37*/  {'*', '*'},
/*0x38*/  {alt_l_char, alt_l_char},
/*0x39*/  {' ', ' '},
/*0x3A*/  {caps_lock_char, caps_lock_char},
};


struct ioqueue kbd_buf; //定义键盘缓冲区

/*键盘中断处理程序*/
static void intr_keyboard_handler(void){
	int ctrl_down_last = ctrl_status;
	int shift_down_last = shift_status;
	int caps_lock_last = caps_lock_status;

	int break_code;
	/*必须要读取输出缓冲区寄存器，否则8042不在继续响应键盘中断*/
	uint16_t scancode = inb(KBD_BUF_PORT);
	
	//put_int(scancode);	

	/*若扫描码scancode是e0开头的，表示此键按下将产生多个扫描码，所以中断该处理函数等待下一个扫描码*/
	if(scancode == 0xe0){
		ext_scancode = 1; //打开e0标记
		return;
	}

	/*如果上次是以e0开头的，将扫描码合并*/
	if(ext_scancode == 1){
		scancode = ((0xe000) | scancode);
		ext_scancode = 0;  //关闭e0标记
	}

	break_code = ((scancode & 0x0080) != 0 ? 1 : 0);
	
	//如果断码是break_code
	if(break_code == 1){
		/*ctrl_r和alt_r的make_code都是两个字节，多字节的make_code暂不处理*/
		uint16_t make_code = scancode & 0xff7f;

		/*若以下3个键被弹起，则设置为false*/
		if(make_code == ctrl_l_make || make_code == ctrl_r_make){
			ctrl_status = 0;
		} else if(make_code == shift_l_make || make_code == shift_r_make){
			//put_str("hahaha");
			shift_status = 0;
		}else if(make_code == alt_l_make || make_code == alt_r_make){
			alt_status = 0;
		}
		/*caps_lock不是弹起后关闭，所以无需处理*/
		return ;
	}
	//alt_r_make和ctrl_r_make都是4个字节
	if((scancode > 0x00 && scancode < 0x3b) || \
			(scancode == alt_r_make) || \
			(scancode == ctrl_r_make)){
		int shift = 0;  //判断是否与shift相结合，用来在一维数组中索引对应的字符
		if((scancode < 0x0e) || (scancode == 0x29) ||\
			(scancode == 0x1a) || (scancode == 0x1b) || \
			(scancode == 0x2b) || (scancode == 0x27) || \
			(scancode == 0x28) || (scancode == 0x33) || \
			(scancode == 0x34) || (scancode == 0x35)){
			if(1 == shift_down_last){
				shift = 1;
			} 
		}else{
			if(shift_down_last == 1&& caps_lock_last == 1){
				shift = 0;
			}else if(shift_down_last == 1|| caps_lock_last == 1){
				shift = 1;
			}else{
				shift = 0;
			}
		}
		uint8_t index = (scancode &= 0x00ff);  //将扫描码高字节置0

		char cur_char = key_map[index][shift]; //在数组中找到对应的字符

		/*只处理ASCII码不为0的键*/
		if(cur_char){
			if(!ioq_full(&kbd_buf)){
				//put_char(cur_char);
				ioq_putchar(&kbd_buf, cur_char);

			}
			return;
		}

		/*记录本次是否按下了下面的几类控制键之一*/
		if(scancode == ctrl_l_make || scancode == ctrl_r_make){
			ctrl_status = 1;
		}else if(scancode == shift_l_make || scancode == shift_r_make){
			shift_status = 1;
		}else if(scancode == caps_lock_make){
			caps_lock_status = caps_lock_status == 1 ? 0 : 1;
		}
	}else{
		put_str("unknown key\n");
	}
	
}


/*键盘初始化*/
void keyboard_init(void){
	put_str("keyboard init start\n");
	ioqueue_init(&kbd_buf);
	register_handler(0x21, intr_keyboard_handler);
	put_str("keyboard init done\n");
}
