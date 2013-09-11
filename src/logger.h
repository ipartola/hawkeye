#ifndef __LOGGER_H
#define __LOGGER_H

#define LOG_ERROR 0
#define LOG_WARNING 1
#define LOG_INFO 2
#define LOG_DEBUG 3

void open_log(const char *filename, short log_level);
void log_it(const short type, const char *str);
void log_itf(const short type, const char *fmt, ...);
void close_log();

#define DBG(...) log_itf(LOG_DEBUG, __VA_ARGS__);

#endif
