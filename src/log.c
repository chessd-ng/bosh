
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

