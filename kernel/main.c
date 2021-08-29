#include "print.h"
#include "keyboard.h"
#include "interrupt.h"
#include "debug.h"
#include "init.h"
#include "thread.h"

void consumer_a(void *);
void consumer_b(void *);

int main(void){
	init_all();
	thread_start("k_thread_a", 31, consumer_a, "A_ ");
	thread_start("k_thread_b", 31, consumer_b, "B_ ");
	intr_enable();
	while(1){console_put_str("main ");}
	return 0;
}


void consumer_a(void *arg){
	while(1){
		enum intr_status old_status = intr_disable();
		if(!ioq_empty(&kbd_buf)){
			console_put_str(arg);
			char byte = ioq_getchar(&kbd_buf);
			console_put_char(byte);
			put_str("  ||  ");
		}
		intr_set_status(old_status);
	}
}


void consumer_b(void *arg){
	while(1){
		enum intr_status old_status = intr_disable();
		if(!ioq_empty(&kbd_buf)){
			console_put_str(arg);
			char byte = ioq_getchar(&kbd_buf);
			console_put_char(byte);
			put_str("  ||  ");
		}
		intr_set_status(old_status);
	}
}
