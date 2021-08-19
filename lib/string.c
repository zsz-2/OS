#include "global.h"
#include "string.h"
#include "debug.h"


void memset(void *dst_, uint8_t value, uint32_t size);
void memcpy(void *dst_, const void *src_, uint32_t size);
int memcpy(const void *a_,  const void *b_, uint32_t size);
char *strcpy(char *dst_, const char *src_);
uint32_t strlen(const char *str);
int8_t strcmp(const char *a,const char *b);
char *strchr(const char *str, const uint8_t ch);
char *strrchr(const char *str, const uint8_t ch);
char *strcat(char *dst_,const char *src_);
uint32_t strchrs(const char *str, uint8_t ch);


/*将dst起始的size字节设置为value*/
void memset(void *dst_, uint8_t value, uint32_t size){
	ASSERT (dst != NULL);
	uint8_t *dst = dst_;
	while(size-- > 0){
		*dst++ = value; 
	}
}

/*将src_起始的size字节复制到dst_*/
void memcpy(void *dst_, const void *src_, uint32_t size){
	ASSERT (dst_ != NULL && src_ != NULL);
	uint8_t *dst = dst_;
	const uint8_t *src = src_;
	while(size-- > 0){
		*dst++ = *src++;
	}
}

/*连续比较以地址a_和地址b_开头的size个字节，相等返回0，大于返回1，小于返回-1*/
int memcpy(const void *a_,  const void *b_, uint32_t size){
	const char * a = a_, *b = b_;
	ASSERT(a != NULL && b != NULL);
	while(size-- > 0){
		if(*a != *b){
			return *a > *b ? 1: -1;
		}
		++a; ++b;
	}
	return 0;
}

/*将字符串从src_复制到dst_*/
char *strcpy(char *dst_, const char *src_){
	ASSERT(dst_ != NULL && src_ != NULL);
	char *r = dst_;
	while((*dst_++ = *src_++));
	return r;
}

/*返回字符串的长度*/
uint32_t strlen(const char *str){
	ASSERT(str != NULL);
	const char *p = str;
	while(*p++);
	return (p - str -1);
}


/*从左到右查找str字符串中字符ch第一次出现的位置*/
char *strchr(const char *str, const uint8_t ch){
	ASSERT(str != NULL);
	while(*str != 0){
		if(*str == ch){
			return (char *)str; //否则会报const属性丢失
		}
		++str;
	}
	return NULL;
}

/*比较两字符串，相等返回0，a大返回1，小返回—1*/
int8_t strcmp(const char *a,const char *b){
	ASSERT(a != NULL && b!= NULL);
	while(*a != 0 &&  *a == *b){
		++a; ++b;
	}
	return *a < *b ? -1 : *a  > *b;
}

/*从后往前查找字符串str中首次出现字符ch的地址*/
char *strrchr(const char *str, const uint8_t ch){
	ASSERT(str != NULL);
	const char *last_char = NULL;
	while(*str != 0){
		if(*str == ch){
			last_char = str;
		}
		++str;
	}
	return (char *)last_char;
}

/*将字符串拼接在src后*/
char *strcat(char *dst_,const char *src_){
	ASSERT(dst_ != NULL && src_ != NULL);
	char *str = dst_;
	while(*str++);
	--str;
	while((*str++ = *src_++));
	return dst_;
}

/*在字符串str中查找ch字符出现的次数*/
uint32_t strchrs(const char *str, uint8_t ch){
	ASSERT(str != NULL);
	uint32_t ch_cnt = 0;
	const char *p = str;
	while(*p != 0){
		if(*p == ch){
			++ch_cnt;
		}
		++p;
	}
	return ch_cnt;
}

