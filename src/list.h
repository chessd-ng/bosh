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


#ifndef lista_h
#define lista_h

/* Macro used to iterate through list elements */
#define list_foreach(it, l) for(it = list_begin(l); it != list_end(l); it = list_next(it))

/*! \brief A callback function that receies one element of the list */
typedef void(*proc)(void *v);

/*! \brief A callback function that compares two elements in the list */
typedef int(*compare_function)(void *v1,void *v2);


struct _list_node;
typedef struct _list_node _list_node;

struct list;
typedef struct list list;

/*! \brief The list iterator's */
typedef _list_node* list_iterator;

/*! \brief Create a new list */
list* list_new();

/*! \brief Destroy a list
 *
 * \param l is the list to be destroyed
 * \param delete_function is a callback used
 *          to free the elements in the list,
 *          is NULL it will not be used.
 */
void list_delete(list *l, proc delete_function);

/*! \brief Insert an element in the end of the list */
list_iterator list_push_back(list* l, void *v);

/*! \brief Remove and return the last element */
void* list_pop_back(list* l);

/*! \brief Insert an element in the begining of the list */
list_iterator list_push_front(list* l,void *v);

/*! \brief Remove and return the first element of the list */
void *list_pop_front(list* l);

/*! \brief Returns the first element */
void* list_front(list* l);

/*! \brief Returns the last element */
void* list_back(list* l);

/*! \brief Return an iterator to the first element */
list_iterator list_begin(list* l);

/*! \brief Return an iterator to the end of the list
 *
 * The returned iterator is not dereferenceable.
 */
list_iterator list_end(list* l);

/*! \breif Returns the iterator to the next element */
list_iterator list_next(list_iterator iterator);

/*! \brief Returns the element pointed by the iterator */
void* list_iterator_value(list_iterator iterator);

/*! \brief Erases the element pointed by the iterator 
 *
 * \return Returns the removed element
 */
void* list_erase(list_iterator iterator);

/*! \brief Returns non-zero if the list is empty */
int list_empty(list* l);

/*! \brief Erase all elements of the list */
void list_clear(list* l, proc delete_function);

/*! \brief Insert an element after the element pointed by the iterator*/
list_iterator list_insert(list_iterator pos, void* v);

/*! \breif Find an element in the list
 *
 * \param l is the list
 * \param v is the element to be found
 * \param compare_function is the function to be used to compare v to the elements in the list.
 */
list_iterator list_find(list* l, void* v, compare_function func);

#endif
