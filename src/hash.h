#ifndef HASH_H
#define HASH_H

typedef unsigned int (*hash_function)(void*);
typedef int (*hash_compare_function)(void*, void*);
typedef void (*hash_callback)(void*, void*);

struct hash;
typedef struct hash hash;

hash* hash_new(hash_function, hash_compare_function);

void hash_delete(hash*);

void hash_clear(hash*);

void hash_insert(hash*, void* key, void* value);

void* hash_insert_replace(hash*, void* key, void* value);

void* hash_find(hash*, void* key);

void* hash_erase(hash*, void* key);

int hash_has_key(hash*, void* key);

unsigned int hash_size(hash*);

void hash_iterate(hash*, hash_callback);

#endif
