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


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "log.h"

void _log(const char* function_name, const char* format, ...) {
    va_list args;
    char* new_format = NULL;
    va_start(args, format);
    asprintf(&new_format, "%s: %s\n", function_name, format);
    vfprintf(stderr, new_format, args);
    free(new_format);
}

