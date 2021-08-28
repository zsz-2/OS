#include "print.h"
#include "interrupt.h"
#include "debug.h"
#include "init.h"
#include "thread.h"

void k_thread_a(void *);
void k_thread_b(void *);

int main(void){
	init_all();
	intr_enable();
	thread_start("k_thread_a", 10, k_thread_a, "argA ");

	thread_start("k_thread_b", 10, k_thread_b, "argB ");

	while(1){
		intr_disable();
		console_put_str("main");
		intr_enable();
	}
	return 0;
}


void k_thread_a(void *arg){
	char *para = arg;
	while(1){
		intr_disable();
		console_put_str(arg);
		intr_enable();
	}
}

void k_thread_b(void *arg){
	char *para = arg;
	while(1){
		intr_disable();
		console_put_str(arg);
		intr_enable();
	}
}


