/******************************************************
  b--输出寄存器QImode名称，即寄存器的最低8位
  w--输出寄存器HImode名称，寄存器中两个字节的部分
  HImode  表示一个两字节的整数
  QImode  表示一个一字节的整数
******************************************************/
#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"

/*保证快速*/
//向端口port写入一个字节
static inline void outb(uint16_t port, uint8_t data){
	//N表示0~255,d表示用寄存器edx/dx/dl/dh.%w1限制了寄存器位dx
	asm volatile ("outb %b0,%w1"::"a"(data),"d"(port));
}

//将addr处起始的word_cnt个字写入端口port
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt){
	asm volatile ("cld; rep outsw;":"+S"(addr),"+c"(word_cnt):"d"(port));
}

//将从端口port读入的一个字节返回
static inline uint8_t inb(uint16_t port){
	uint8_t data;
	asm volatile ("inb %w1,%b0;":"=a"(data):"Nd"(port));
	return data;
}

//将从端口读入的word_cnt个字写入addr
static inline void insw(uint16_t port, const void* addr, uint32_t word_cnt){
	asm volatile("cld;rep insw;":"+D"(addr),"+c"(word_cnt):"Nd"(port):"memory");
}


#endif
