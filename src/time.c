#include "time.h"

#include <time.h>

time_type get_time() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec * 1000ll + tp.tv_nsec / 1000000ll;
}

