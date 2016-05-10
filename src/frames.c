
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "server.h"
#include "memory.h"
#include "logger.h"

#include "frames.h"

void create_frame_buffer(struct frame_buffer *fb, size_t n) {
    int i;

    fb->buffer_size = n;
    fb->current_frame = -1;
    fb->frames = calloc(n, sizeof(struct frame));
    fb->vd = NULL;

    for (i = 0; i < fb->buffer_size; i++) {
        fb->frames[i].data = malloc(MIN_FRAME_SIZE);
        fb->frames[i].data_len = 0;
        fb->frames[i].data_buf_len = MIN_FRAME_SIZE;
    }
}

void destroy_frame_buffer(struct frame_buffer *fb) {
    int i;

    for (i = 0; i < fb->buffer_size; i++) {
        free(fb->frames[i].data);
    }

    free(fb->frames);
}

void add_frame(struct frame_buffer *fb, void *data, size_t data_len) {
    unsigned long frame_num;
    struct frame *f;
    size_t total_data_len;

    total_data_len = data_len + strlen(FRAME_HEADER) + strlen(FRAME_FOOTER);

    fb->current_frame++;
    frame_num = fb->current_frame % fb->buffer_size;
    f = &fb->frames[frame_num];

    if (f->data_buf_len < total_data_len) {
        if (data_len <= MAX_FRAME_SIZE) {
            f->data = realloc(f->data, total_data_len);
            f->data_buf_len = total_data_len;
        }
        else {
            log_itf(LOG_WARNING, "Could not realloc frame data because frame is larger than MAX_FRAME_SIZE: data_len = %d", total_data_len);
            return;
        }
    }

    memcpy(f->data, FRAME_HEADER, strlen(FRAME_HEADER));
    memcpy(&f->data[strlen(FRAME_HEADER)], data, data_len);
    memcpy(&f->data[data_len + strlen(FRAME_HEADER)], FRAME_FOOTER, strlen(FRAME_FOOTER));
    f->data_len = total_data_len;
}

struct frame *get_frame(struct frame_buffer *fb, unsigned long index) {

    if (index <= fb->current_frame - fb->buffer_size) {
        return NULL;
    }

    return &fb->frames[index % fb->buffer_size];
}

