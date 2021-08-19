#ifndef __KERNEL_DEBUG_H
#define __KERNWL_DEBUG_H

void panic_spin(char *filename, int line, const char *func, const char *condition);

/*---------------------------------__VA_ARG__------------------------------------------*
 *__VA_ARGS__是预处理器所支持的专用标识符号
 */
#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)
/*-------------------------------------------------------------------------------------*/
/*符号#让宏的参数转化为字符串字面量*/
#ifdef NDEBUG
	#define ASSERT(CONDITION) ((void) 0)
#else
#define ASSERT(CONDITION)\
	if(CONDITION) {}else{ \
		PANIC(#CONDITION);\
}
	

#endif  /*__NDEBUG*/

#endif  /*__KERNEL_DEBUG_H*/
