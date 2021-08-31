#include "print.h"
#include "userprog.h"
#include "keyboard.h"
#include "interrupt.h"
#include "debug.h"
#include "init.h"
#include "thread.h"

/*
void consumer_a(void *);
void consumer_b(void *);
*/

void k_thread_a(void *);
void k_thread_b(void *);
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0, test_var_b = 0;

int main(void){
	init_all();

	//thread_start("k_thread_a", 31, k_thread_a, "A_ ");
	//thread_start("k_thread_b", 31, k_thread_b, "B_ ");
	process_execute(u_prog_a, "user_prog_a");

	//process_execute(u_prog_b, "user_prog_b");
	//intr_enable();
	while(1){};

	return 0;
}

/*
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
*/


void k_thread_a(void *arg){
	char *para = arg;
	while(1){
		console_put_str("v_a:0x");
		console_put_int(test_var_a);
		console_put_str("\n");
	}
}


void k_thread_b(void *arg){
	char *para = arg;
	while(1){
		console_put_str("v_b:0x");
		console_put_int(test_var_b);
		console_put_str("\n");
	}
}

void u_prog_a(void){
	intr_disable();
	while(1){
		//console_put_str("noooooooooooooooos\n");
		++test_var_a;
	}
}


void u_prog_b(void){
	while(1){
		++test_var_b;
	}
}
