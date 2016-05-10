/*******************************************************************************
# Linuc-UVC streaming input-plugin for MJPG-streamer                           #
# Modified by Igor Partola to be more generic,                                 # 
# with some functionality removed.                                             #
#                                                                              #
# This package work with the Logitech UVC based webcams with the mjpeg feature #
#                                                                              #
# Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard                   #
#                    2007 Lucas van Staden                                     #
#                    2007 Tom Stöveken                                         #
#                    2013 Igor Partola                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#ifndef V4L2_UVC_H
#define V4L2_UVC_H


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>

#define MAX_DEVICE_FILENAME 32

#define NB_BUFFER 4

#define IOCTL_RETRY 4

#ifdef DISABLE_LIBV4L2
#define IOCTL_VIDEO(fd, req, value) ioctl(fd, req, value)
#define OPEN_VIDEO(fd, flags) open(fd, flags)
#define CLOSE_VIDEO(fd) close(fd)
#else
#include <libv4l2.h>
#define IOCTL_VIDEO(fd, req, value) v4l2_ioctl(fd, req, value)
#define OPEN_VIDEO(fd, flags) v4l2_open(fd, flags)
#define CLOSE_VIDEO(fd) v4l2_close(fd)
#endif

#define MIN_BYTES_USED 0xaf

#define UVC_FALLBACK_FORMAT V4L2_PIX_FMT_YUYV

typedef struct _control control;
struct _control {
    struct v4l2_queryctrl ctrl;
    int value;
    struct v4l2_querymenu *menuitems;
    /* In the case the control a V4L2 ctrl this variable will specify
     * that the control is a V4L2_CTRL_CLASS_USER control or not.
     * For non V4L2 control it is not acceptable, leave it 0.
     */
    int class_id;
};

enum _streaming_state {
    STREAMING_OFF = 0,
    STREAMING_ON = 1,
    STREAMING_PAUSED = 2,
};
typedef enum _streaming_state streaming_state;

struct resolution {
    unsigned int width;
    unsigned int height;
    uint32_t pixelformat; // Corresponds to the pixelformat found in struct v4l2_fmtdesc
};

struct video_device {
    int fd;
    char *device_filename;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers rb;
    void *mem[NB_BUFFER];
    unsigned char *framebuffer;
    size_t framebuffer_size;
    streaming_state streaming_state;
    int use_streaming;
    int width;
    int height;
    int fps;
    int format_in;
    int jpeg_quality;

    struct v4l2_fmtdesc *formats;
    unsigned int format_count;
    unsigned int current_format_index;

    struct resolution *resolutions;
    unsigned int resolution_count;
    unsigned int current_resolution_index;

};

int init_v4l2(struct video_device *vd);

struct video_device *create_video_device(char *device, int width, int height, int fps, int format, int jpeg_quality);
void destroy_video_device(struct video_device *vd);

size_t copy_frame(unsigned char *dst, const size_t dst_size, unsigned char *src, const size_t src_size);
size_t capture_frame(struct video_device *vd);
int requeue_device_buffer(struct video_device *vd);

#endif
