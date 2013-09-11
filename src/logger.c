#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "memory.h"
#include "logger.h"

static FILE *log_file;
static short max_log_level;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static char log_level_labels[][16] = {
	"ERROR",
	"WARNING",
	"INFO",
	"DEBUG"
};


void open_log(const char *filename, short log_level) {
	if (strlen(filename) == 0) {
		log_file = stdout;
	}
	else {
		log_file = fopen(filename, "a");
		if (log_file == NULL) {
			panic("Could not open log file.");
		}
	}

    max_log_level = log_level;
}

void log_it(const short type, const char *str) {
	time_t lt = time(NULL);
	struct tm *now = localtime(&lt);

    if (type > max_log_level) {
        return;
    }
	
	pthread_mutex_lock(&log_lock);
	fprintf(log_file, "%d-%02d-%02d %02d:%02d:%02d %s: %s\n", 
			now->tm_year + 1900, 
			now->tm_mon + 1, 
			now->tm_mday, 
			now->tm_hour, 
			now->tm_min, 
			now->tm_sec,
			log_level_labels[type],
			str
			);

	fflush(log_file);

	pthread_mutex_unlock(&log_lock);
}

void log_itf(const short type, const char *fmt, ...) {
	time_t lt = time(NULL);
	struct tm *now = localtime(&lt);
    
    if (type > max_log_level) {
        return;
    }
	
	pthread_mutex_lock(&log_lock);
	fprintf(log_file, "%d-%02d-%02d %02d:%02d:%02d %s: ", 
			now->tm_year + 1900, 
			now->tm_mon + 1, 
			now->tm_mday, 
			now->tm_hour, 
			now->tm_min, 
			now->tm_sec,
			log_level_labels[type]
			);

	va_list args;
	va_start(args, fmt);
	vfprintf(log_file, fmt, args);
	va_end(args);

	fputc('\n', log_file);
	fflush(log_file);
	
	pthread_mutex_unlock(&log_lock);
}

void close_log() {
	fclose(log_file);
	pthread_mutex_destroy(&log_lock);
}
