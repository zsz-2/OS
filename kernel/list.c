#include "interrupt.h"
#include "print.h"
#include "list.h"

#define NULL 0
void list_init(struct list* list);
struct list_elem *list_pop(struct list *plist);
int list_empty(struct list *plist);
void list_remove(struct list_elem *pelem);
int ele_find(struct list *plist ,struct list_elem *obj_elem);
void list_append(struct list *plist, struct list_elem *elem);
void list_insert_before(struct list_elem *elem, struct list_elem *before);
void list_push(struct list *plist, struct list_elem *elem);
uint32_t list_len(struct list* plist);
struct list_elem *list_traversal(struct list *plist, function func, int arg);

	
	
	
/*初始化双向链表*/
void list_init(struct list* list){
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}	


/*把链表元素elem插在元素before之前*/
void list_insert_before(struct list_elem *elem, struct list_elem *before){
	enum intr_status  old_status = intr_disable();

	/*将before的前驱元素的后继元素更新为elem*/
	before->prev->next = elem;
	elem->prev = before->prev;
	elem->next = before;
	before->prev = elem;

	intr_set_status(old_status);
}

/*添加元素到列表队首，类似栈push操作*/
void list_push(struct list *plist, struct list_elem *elem){
	list_insert_before(elem, plist->head.next);
}

/*添加元素到列表队尾*/
void list_append(struct list *plist, struct list_elem *elem){
	list_insert_before(elem, &plist->tail);
}

/*删除元素pelem*/
void list_remove(struct list_elem *pelem){
	enum intr_status old_status = intr_disable();
	
	pelem->prev->next = pelem->next;
	pelem->next->prev = pelem->prev;

	intr_set_status(old_status);
}

/*将链表的第一个元素弹出并删除*/
struct list_elem *list_pop(struct list *plist){
	struct list_elem *elem = (plist->head).next;
	list_remove(elem);
	return elem;
}

/*从列表中查找元素obj_elem.成功返回1，失败返回0*/
int elem_find(struct list *plist ,struct list_elem *obj_elem){
	struct list_elem *elem = plist->head.next;
       	while((elem != &plist->tail)){
		if(elem == obj_elem) return 1;
		elem = elem->next;
	}	
	return 0;
}

/*查找列表中有没有符合func函数的*/
struct list_elem *list_traversal(struct list *plist, function func, int arg){
	struct list_elem *elem = plist->head.next;
	if(list_empty(plist) == 1) return NULL;
	while(elem != &plist->tail){
		if(func(elem, arg) == 1){
			return elem;
		} 
		elem = elem->next;
	}
	return NULL;
}

/*返回链表的长度*/
uint32_t list_len(struct list* plist){
	if(plist == NULL) return 0;

	struct list_elem *elem= plist->head.next;
	uint32_t length = 0;
	/*
	put_str("list_len    ");
	put_int((uint32_t)&plist->tail);
	put_str("\n");*/
	while(elem != &plist->tail){
		//put_int((uint32_t)elem);
		//put_str("\n");
		++length;
		elem = elem->next;
	}
	//put_str("list_len\n");
	return length;
}

/*判断链表是否为空，空返回1，非空返回0*/
int list_empty(struct list *plist){
	//put_str("list_empty \n");
	return (list_len(plist) == 0) ? 1: 0;
}
