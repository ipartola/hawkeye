
#ifndef __FRAMES_H
#define __FRAMES_H

#include "v4l2uvc.h"

#define MIN_FRAME_SIZE 8*1024
#define MAX_FRAME_SIZE 1024*1024
#define MAX_HEADER_LEN 1024

struct frame {
    char *data;
    size_t data_len;
    size_t data_buf_len;
};

struct frame_buffer {
    struct frame *frames;
    long current_frame;
    size_t buffer_size;
    struct video_device *vd;
};

struct frame_buffers {
    struct frame_buffer *buffers;
    size_t count;
};

void create_frame_buffer(struct frame_buffer *fb, size_t n);
void destroy_frame_buffer(struct frame_buffer *fb);
void add_frame(struct frame_buffer *fb, void *data, size_t data_len);
struct frame *get_frame(struct frame_buffer *fb, unsigned long index);

#endif
