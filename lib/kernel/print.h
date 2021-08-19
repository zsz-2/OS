#ifndef __LIB__KERNEL_PRINT_H
#define __LIB__KERNEL_PRINT_H
#include "stdint.h"
void put_char(uint8_t char_asci);
void put_string(char* message);
void put_int(uint32_t num);
#endif
