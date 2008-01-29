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
