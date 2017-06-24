
#ifndef UTIL_H
#define UTIL_H

#define max(a,b) \
    ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
    ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
     _a < _b ? _a : _b; })

void double_to_timeval(double d, struct timeval *tv);
void double_to_timespec(double d, struct timespec *ts);
double gettime();
short is_blank(const char c);
void trim(char *c);
int file_exists(const char *filename);
ssize_t file_size(const char *filename);
char *base64_encode(const unsigned char *data);

#define is_lws(c) (c == ' ' || c == '\t')

#endif
