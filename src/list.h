#ifndef lista_h
#define lista_h

#define list_foreach(it, l) for(it = list_begin(l); it != list_end(l); it = list_next(it))

typedef void(*proc)(void *v);
typedef int(*compare_function)(void *v1,void *v2);

typedef struct _list_node {
	struct _list_node *next, *prev;
	void *value;
} _list_node;

typedef struct list {
	_list_node *head;
} list;

typedef _list_node* list_iterator;

list* list_new();
void list_delete(list *l, proc delete_function);

list_iterator list_push_back(list* l, void *v);
void* list_pop_back(list* l);

list_iterator list_push_front(list* l,void *v);
void *list_pop_front(list* l);

void* list_front(list* l);
void* list_back(list* l);

list_iterator list_begin(list* l);
list_iterator list_end(list* l);
list_iterator list_next(list_iterator iterator);

void* list_iterator_value(list_iterator iterator);

void* list_erase(list_iterator iterator);

int list_empty(list* l);

void list_clear(list* l, proc delete_function);

list_iterator list_insert(list_iterator pos, void* v);

list_iterator list_find(list* l, void* v, compare_function func);

#endif
