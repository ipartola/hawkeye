
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

double gettime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec + ((double) tv.tv_usec) / 1000 / 1000;
}

void double_to_timeval(double d, struct timeval *tv) {
    tv->tv_sec = (int) d;
    tv->tv_usec = (int) ((d - (int) d) * 1000 * 1000);
}

void double_to_timespec(double d, struct timespec *ts) {
    ts->tv_sec = (int) d;
    ts->tv_nsec = (int) ((d - (int) d) * 1000 * 1000 * 1000);
}

short is_blank(const char c) {
	if (c == 0 || c == ' ' || c == '\t' || c == '\n' || c == '\r')
		return 1;

	return 0;
}

void trim(char *c) {
	int i, len;
	len = strlen(c);
	
	for (i = len - 1; i >= 0; i--) {
		if (is_blank(c[i])) {
			c[i] = 0;
		}
		else {
			break;
		}
	}

	for (i = 0; i < len; i++) {
		if (!is_blank(c[i])) {
			c = memmove(&c[0], &c[i], len - i);
			c[len - i] = 0;
			break;
		}
	}
}

int file_exists(const char *filename) {
    struct stat st;
    return (stat(filename, &st) == 0);
}

ssize_t file_size(const char *filename) {
    struct stat st;

    if (stat(filename, &st) < 0) {
        return -1;
    }

    return st.st_size;
}


static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'};

static int mod_table[] = {0, 2, 1};

char *base64_encode(const unsigned char *data) {

    size_t input_length, output_length;
    char *encoded_data;
    uint32_t octet_a, octet_b, octet_c, triple;
    int i, j;

    input_length = strlen((char *) data);

    output_length = 4 * ((input_length + 2) / 3);

    encoded_data = malloc(output_length + 1);
    memset(encoded_data, 0, output_length);

    for (i = 0, j = 0; i < input_length;) {

        octet_a = i < input_length ? data[i++] : 0;
        octet_b = i < input_length ? data[i++] : 0;
        octet_c = i < input_length ? data[i++] : 0;

        triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[input_length % 3]; i++) {
        encoded_data[output_length - 1 - i] = '=';
    }

    encoded_data[output_length] = '\0';

    return encoded_data;
}

