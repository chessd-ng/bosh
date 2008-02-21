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
#include <time.h>

#include <errno.h>
#include <string.h>

#include "log.h"

FILE* log_file = NULL;

void log_set_file(const char* file_name) {
    log_file = fopen(file_name, "r");
    if(log_file == NULL) {
        fprintf(stderr, "Could not open %s for logging: %s\n", file_name, strerror(errno));
        fprintf(stderr, "Switching log output to standard output.\n");
        log_file = stderr;
    }
}

void _log(const char* function_name, const char* format, ...) {
    va_list args;
    char* new_format = NULL;
    char time_str[256];
    time_t t;

    if(log_file == NULL) {
        fprintf(stderr, "Log output not set.\n");
        log_file = stderr;
    }

    t = time(NULL);
    strftime(time_str, 255, "%Y-%m-%d %H:%M:%S", localtime(&t));

    va_start(args, format);
    asprintf(&new_format, "%s %s: %s\n", time_str, function_name, format);
    vfprintf(log_file, new_format, args);
    free(new_format);
}

