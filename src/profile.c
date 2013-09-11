
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

static struct timeval start, end;

void start_time() {
    gettimeofday(&start, NULL);
}

void print_time() {
    gettimeofday(&end, NULL);

    printf("Time delta = %f\n", (double) (end.tv_sec - start.tv_sec) + ((double) (end.tv_usec - start.tv_usec)) / 10000000);
}
