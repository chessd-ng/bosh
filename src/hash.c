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

#include "allocator.h"

#include "hash.h"

/*! \brief A node of the hash table */
typedef struct _hash_node {
	void* key;
	void* value;
	struct _hash_node* next;
} _hash_node;

/*! \brief The hash table */
struct hash {
	_hash_node* table;
	size_t table_size;
	size_t element_count;

	hash_function hf;
	hash_compare_function cf;
};

DECLARE_ALLOCATOR(_hash_node, 4096);
IMPLEMENT_ALLOCATOR(_hash_node);

DECLARE_ALLOCATOR(hash, 32);
IMPLEMENT_ALLOCATOR(hash);

/* borrowed from g++ hash implementation */
static const unsigned long prime_count = 28;

/* A list of good sizes for the hash table */
static const size_t prime_list[] = {
	53ul,         97ul,         193ul,        389ul,       769ul,
	1543ul,       3079ul,       6151ul,       12289ul,     24593ul,
	49157ul,      98317ul,      196613ul,     393241ul,    786433ul,
	1572869ul,    3145739ul,    6291469ul,    12582917ul,  25165843ul,
	50331653ul,   100663319ul,  201326611ul,  402653189ul, 805306457ul,
	1610612741ul, 3221225473ul, 4294967291ul
};

/*! \brief Creates a new hash table. */
hash* hash_new(hash_function hf, hash_compare_function cf) {
	hash* h;

    h = hash_alloc();

	h->hf = hf;
	h->cf = cf;

	h->element_count = 0;
	h->table_size = prime_list[0];

    /* I'm using a dummy head node for each list */
	h->table = calloc(h->table_size, sizeof(_hash_node));

	return h;
}

/*! \brief Creates a new hash node. */
static _hash_node* node_new(void* key, void* value) {
	_hash_node* node;

    node = _hash_node_alloc();

	node->key = key;
	node->value = value;
	node->next = NULL;

	return node;
}

/*! \brief Insert a node to the hash and don't resize the table. */
static void hash_insert_noresize(hash* h, _hash_node* node) {
	size_t hash_pos;

	hash_pos = h->hf(node->key) % h->table_size;

	node->next = h->table[hash_pos].next;
    h->table[hash_pos].next = node;
}

/*! \brief Resize the table if necessary. */
static void hash_resize(hash* h) {
	_hash_node* old_table;
	size_t old_size;
	_hash_node* node;
	int i;

	if(h->table_size < h->element_count) {
		old_table = h->table;
		old_size = h->table_size;

		for(i = 0; prime_list[i] < h->element_count; ++i) ;

		h->table_size = prime_list[i];
		h->table = calloc(h->table_size, sizeof(_hash_node));

		for(i = 0; i < old_size; ++i) {
			while(old_table[i].next != NULL) {
				node = old_table[i].next;
				old_table[i].next = node->next;
				hash_insert_noresize(h, node);
			}
		}
		free(old_table);
	}
}

/*! \brief Insert a new item to the hash.
 *
 * \note This function does not check for repeated keys, so if the already exists it will keep both elements.
 */
void hash_insert(hash* h, void* key, void* value) {
	_hash_node* node;

	node = node_new(key, value);

	hash_insert_noresize(h, node);
	h->element_count ++;

	hash_resize(h);
}

/*! \brief Find a node in the table by its key
 *
 * \return Returns the node whose next node has the requested key or NULL if the key is not found.
 * */
static _hash_node* hash_find_prev_node(hash* h, void* key) {
	_hash_node* node;
	size_t hash_pos;

	hash_pos = h->hf(key) % h->table_size;
	
	for(node = &h->table[hash_pos]; node->next != NULL; node = node->next) {
		if(h->cf(node->next->key, key))
			return node;
	}

	return NULL;
}

/*! \brief Erase the next node of the given node.
 *
 * \return Returns the node's value.
 * */
void* hash_erase_next_node(hash* h, _hash_node* node) {
    _hash_node* erased_node;
    void* value;

    erased_node = node->next;
    node->next = erased_node->next;
    value = erased_node->value;

    _hash_node_free(erased_node);

    h->element_count --;

    return value;
}

/*! \brief Find a value in the table by its key
 *
 * \return Returns the value or NULL if the key was not found
 */
void* hash_find(hash* h, void* key) {
	_hash_node* node;

	node = hash_find_prev_node(h, key);

	if(node == NULL) {
		return NULL;
	} else {
		return node->next->value;
	}
}

/*! \brief Erase an element of the table by its key
 *
 * \returns Returns the value of the erased element or NULL if the key was not found
 */
void* hash_erase(hash* h, void* key) {
	_hash_node* node;

    node = hash_find_prev_node(h, key);

	if(node == NULL) {
		return NULL;
	} else  {
        return hash_erase_next_node(h, node);
    }
}

/*! \brief Erase all elements of table */
void hash_clear(hash* h) {
	int i;

	for(i = 0; i < h->table_size; ++i) {
		while(h->table[i].next != NULL) {
            hash_erase_next_node(h, &h->table[i]);
		}
	}
}

/*! \bief Delete a hash table */
void hash_delete(hash* h) {
	hash_clear(h);
	free(h->table);
	hash_free(h);
}

/*! \brief Ask if the table contain a given key. */
int hash_has_key(hash* h, void* key) {
	return hash_find(h, key) != NULL;
}

/*! \brief The size of the table */
size_t hash_size(hash* h) {
	return h->element_count;
}


/*! \brief Insert a element or replace an elemente if the already exists
 *
 * \return Returns the value of the replaced element or NULL if the key didn't exist.
 */
void* hash_insert_replace(hash* h, void* key, void* value) {
	_hash_node* node;
	void* v;

	node = hash_find_prev_node(h, key);
	if(node == NULL) {
		v = NULL;
		hash_insert(h, key, value);
	} else {
        node = node->next;
		v = node->value;
		node->value = value;
        // FIXME
        // node->key = key
	}

	return v;
}

/*! \brief Iterate through all elements in the hash table
 *
 * The function callback is called for every element in the table
 */
void hash_iterate(hash* h, hash_callback callback) {
	int i;
	_hash_node* node;

	for(i = 0; i < h->table_size; ++i) {
		for(node = h->table[i].next; node != NULL; node = node->next) {
			callback(node->key, node->value);
		}
	}
}
