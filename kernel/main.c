#include "print.h"
#include "debug.h"
#include "init.h"

void main(void){
	put_str("I am a kernel\n");
	init_all();
	//asm volatile("sti");
	put_str("time to assert\n");
	while(1);
}

//
//

