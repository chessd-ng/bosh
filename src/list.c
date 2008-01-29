/*
 *   Copyright (c) 2007-2008 C3SL.
 *
 *   This file is part of Bosh.
 *
 *   Bosh is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   any later version.
 *
 *   Bosh is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 */


#include <stdlib.h>
#include <assert.h>
#include "list.h"

/*! \brief A node in the list */
struct _list_node {
	struct _list_node *next, *prev;
	void *value;
};

/*! \brief The list */
struct list {
	_list_node head;
};

_list_node* _list_node_new(void* value) {
    _list_node* node = malloc(sizeof(_list_node));
    node->next = node->prev = NULL;
    node->value = value;
    return node;
}

list* list_new() {
	list *l = malloc(sizeof(list));
	if(l == NULL)
		return NULL;
    l->head.next = l->head.prev = &l->head;
	return l;
}

list_iterator list_insert(list_iterator pos, void* v) {
    _list_node* node = _list_node_new(v);

    node->next = pos;
    node->prev = pos->prev;
    node->next->prev = node;
    node->prev->next = node;

	return node;
}

void* list_erase(list_iterator it) {
	
    void* value = it->value;
    it->prev->next = it->next;
    it->next->prev = it->prev;
    free(it);

    return value;
}

list_iterator list_push_back(list* l, void *v) {
    return list_insert(&l->head, v);
}

list_iterator list_push_front(list* l, void *v) {
    return list_insert(l->head.next, v);
}

void* list_pop_back(list* l) {
    return list_erase(l->head.prev);
}

void* list_pop_front(list* l) {
    return list_erase(l->head.next);
}

list_iterator list_begin(list* l) {
    return l->head.next;
}

list_iterator list_end(list* l) {
    return &l->head;
}

list_iterator list_next(list_iterator iterator) {
    return iterator->next;
}

void* list_iterator_value(list_iterator iterator) {
    return iterator->value;
}

int list_empty(list* l) {
    return &l->head == l->head.next;
}

void list_clear(list* l, proc delete_function) {
    void* v;

    while(!list_empty(l)) {
        v = list_pop_front(l);
        if(delete_function != NULL)
            delete_function(v);
    }
}

void list_delete(list *l, proc delete_function) {
    list_clear(l, delete_function);
    free(l);
}

list_iterator list_find(list* l, void* v, compare_function func) {
    _list_node* node;

    for(node = l->head.next; node != &l->head; node = node->next) {
        if(func(node->value, v)) {
            return node;
        }
    }
    return list_end(l);
}

void* list_front(list* l) {
    return l->head.next->value;
}

void* list_back(list* l) {
    return l->head.prev->value;
}
