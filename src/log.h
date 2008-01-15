#ifndef LOG_H
#define LOG_H

#define log(...) _log(__func__, __VA_ARGS__)

void _log(const char* function_name, const char* format, ...);

#endif
