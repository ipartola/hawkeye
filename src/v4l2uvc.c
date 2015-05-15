/*******************************************************************************
# Linux-UVC streaming input-plugin for MJPG-streamer                           #
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

#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

#include "huffman.h"
#include "logger.h"
#include "memory.h"

#include "v4l2uvc.h"

static int xioctl(int fd, int IOCTL_X, void *arg);
static int video_enable(struct video_device *vd);
static int video_disable(struct video_device *vd, streaming_state disabledState);

static int xioctl(int fd, int IOCTL_X, void *arg) {
    int ret = 0;
    int tries = IOCTL_RETRY;

    do {
        ret = IOCTL_VIDEO(fd, IOCTL_X, arg);
    } while(ret && tries-- &&
            ((errno == EINTR) || (errno == EAGAIN) || (errno == ETIMEDOUT)));

    if (ret && tries <= 0) {
        log_itf(LOG_ERROR, "ioctl (%x) retried %i times - giving up: %s.", IOCTL_X, IOCTL_RETRY, strerror(errno));
        user_panic("ioctl error.");
    }

    return (ret);
}

struct video_device *create_video_device(char *device, int width, int height, int fps, int format, int jpeg_quality) {
    struct video_device *vd;
    struct v4l2_fmtdesc fmtdesc;
    int current_width, current_height = 0;
    struct v4l2_format current_format;
    struct v4l2_frmsizeenum fsenum;
    int j;

    vd = malloc(sizeof(struct video_device));

    if (device == NULL)
        return NULL;

    if (width == 0 || height == 0)
        return NULL;

    vd->device_filename = (char *) calloc(MAX_DEVICE_FILENAME, sizeof(char));
    snprintf(vd->device_filename, MAX_DEVICE_FILENAME, "%s", device);

    vd->width = width;
    vd->height = height;
    vd->fps = fps;
    vd->format_in = format;
    vd->use_streaming = 1; // Use mmap
    vd->jpeg_quality = jpeg_quality;
    
    vd->format_count = 0;
    vd->formats = NULL;
    
    vd->resolution_count = 0;
    vd->resolutions = NULL;

    if (init_v4l2(vd) < 0) {
        user_panic("Init V4L2 failed on device %s.", vd->device_filename);
    }

    // enumerating formats
    memset(&current_format, 0, sizeof(struct v4l2_format));
    current_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(vd->fd, VIDIOC_G_FMT, &current_format) == 0) {
        current_width = current_format.fmt.pix.width;
        current_height = current_format.fmt.pix.height;
        DBG("Current resolution is %dx%d on device %s.", current_width, current_height, vd->device_filename);
    }


    while (1) {
        memset(&fmtdesc, 0, sizeof(struct v4l2_fmtdesc));
        fmtdesc.index = vd->format_count;
        fmtdesc.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(vd->fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0) {
            break;
        }

        if (vd->formats == NULL) {
            vd->formats = (struct v4l2_fmtdesc*) malloc(sizeof(struct v4l2_fmtdesc));
        } else {
            vd->formats = (struct v4l2_fmtdesc*) realloc(vd->formats, (vd->format_count + 1)  *sizeof(struct v4l2_fmtdesc));
        }

        memcpy(&vd->formats[vd->format_count], &fmtdesc, sizeof(struct v4l2_fmtdesc));

        if (fmtdesc.pixelformat == format) {
            vd->current_format_index = vd->format_count;
        }
        
        DBG("%s: Supported format: %s", vd->device_filename, fmtdesc.description);

        memset(&fsenum, 0, sizeof(struct v4l2_frmsizeenum));
        for (j = 0; ; j++) {
            fsenum.pixel_format = fmtdesc.pixelformat;
            fsenum.index = j;
            
            if (xioctl(vd->fd, VIDIOC_ENUM_FRAMESIZES, &fsenum) != 0) {
                break; // Stop enumeration
            }

            if (vd->resolutions == NULL) {
                vd->resolutions = malloc(sizeof(struct resolution));
            }
            else {
                vd->resolutions = realloc(vd->resolutions, (vd->resolution_count + 1)  *sizeof(struct resolution));
            }

            vd->resolutions[vd->resolution_count].width = fsenum.discrete.width;
            vd->resolutions[vd->resolution_count].height = fsenum.discrete.height;
            vd->resolutions[vd->resolution_count].pixelformat = fmtdesc.pixelformat;
            
            if (format == fmtdesc.pixelformat) {
                if (fsenum.discrete.width == width && fsenum.discrete.height == height) {
                    vd->current_resolution_index = vd->resolution_count;
                }
            }
            
            DBG("%s: supported size: %dx%d", vd->device_filename, fsenum.discrete.width, fsenum.discrete.height);
            
            vd->resolution_count += 1;
        }

        vd->format_count++;
    }

    switch(vd->format_in) {
        case V4L2_PIX_FMT_MJPEG:
            vd->framebuffer_size = vd->width * (vd->height + 8) * 2;
            vd->framebuffer = (unsigned char *) malloc(vd->framebuffer_size);
            break;
        case V4L2_PIX_FMT_YUYV:
            vd->framebuffer_size = vd->width * vd->height * 2;
            vd->framebuffer = (unsigned char *) malloc(vd->framebuffer_size);
            break;
        default:
            user_panic("init_video_in: Unsupported format.");
            break;
    }

    return vd;
}

int init_v4l2(struct video_device *vd) {
    int i;
    struct v4l2_streamparm setfps;

    if ((vd->fd = OPEN_VIDEO(vd->device_filename, O_RDWR)) == -1) {
        log_itf(LOG_ERROR, "Error opening V4L2 interface on %s. errno %d", vd->device_filename, errno);
    }

    memset(&vd->cap, 0, sizeof(struct v4l2_capability));
    
    if (xioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap) < 0) {
        log_itf(LOG_ERROR, "Error opening device %s: unable to query device.", vd->device_filename);
        return -1;
    }

    if ((vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        log_itf(LOG_ERROR, "Error opening device %s: video capture not supported.", vd->device_filename);
        return -1;
    }

    if (vd->use_streaming) {
        if (!(vd->cap.capabilities & V4L2_CAP_STREAMING)) {
            log_itf(LOG_ERROR, "%s does not support streaming I/O", vd->device_filename);
            return -1;
        }
    }
    else {
        if (!(vd->cap.capabilities & V4L2_CAP_READWRITE)) {
            log_itf(LOG_ERROR, "%s does not support read I/O", vd->device_filename);
            return -1;
        }
    }

    vd->streaming_state = STREAMING_OFF;

    // set format in
    memset(&vd->fmt, 0, sizeof(struct v4l2_format));
    vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->fmt.fmt.pix.width = vd->width;
    vd->fmt.fmt.pix.height = vd->height;
    vd->fmt.fmt.pix.pixelformat = vd->format_in;
    vd->fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (xioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt) < 0) {
        log_itf(LOG_WARNING, "Unable to set format %d, res %dx%d, device %s. Trying fallback.", vd->format_in, vd->width, vd->height, vd->device_filename);

        // Try the fallback format
        vd->format_in = UVC_FALLBACK_FORMAT;
        vd->fmt.fmt.pix.pixelformat = vd->format_in;

        if (xioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt) < 0) {
            log_itf(LOG_ERROR, "Unable to set fallback format %d, res %dx%d, device %s.", vd->format_in, vd->width, vd->height, vd->device_filename);
            return -1;
        }
    }

    if ((vd->fmt.fmt.pix.width != vd->width) ||
            (vd->fmt.fmt.pix.height != vd->height)) {
        log_itf(LOG_WARNING, "The format asked unavailable, so the width %d height %d on device %s.", vd->fmt.fmt.pix.width, vd->fmt.fmt.pix.height, vd->device_filename);

        vd->width = vd->fmt.fmt.pix.width;
        vd->height = vd->fmt.fmt.pix.height;

        // look the format is not part of the deal ???
        if (vd->format_in != vd->fmt.fmt.pix.pixelformat) {
            if (vd->format_in == V4L2_PIX_FMT_MJPEG) {
                log_itf(LOG_ERROR, "The input device %s does not supports MJPEG mode.\nYou may also try the YUV mode, but it requires a much more CPU power.", vd->device_filename);
                return -1;
            } else if (vd->format_in == V4L2_PIX_FMT_YUYV) {
                log_itf(LOG_ERROR, "The input device %s does not supports YUV mode.", vd->device_filename);
                return -1;
            }
        } else {
            vd->format_in = vd->fmt.fmt.pix.pixelformat;
        }
    }

    // set framerate
    memset(&setfps, 0, sizeof(struct v4l2_streamparm));
    setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps.parm.capture.timeperframe.numerator = 1;
    setfps.parm.capture.timeperframe.denominator = vd->fps;

    if (xioctl(vd->fd, VIDIOC_S_PARM, &setfps) < 0) {
        log_itf(LOG_ERROR, "Unable to set FPS to %d on device %s.", vd->fps, vd->device_filename);
        return -1;
    }


    // request buffers
    memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
    vd->rb.count = NB_BUFFER;
    vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->rb.memory = V4L2_MEMORY_MMAP;

    if (xioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb) < 0) {
        log_itf(LOG_ERROR, "Unable to allocate buffers for device %s.", vd->device_filename);
        return -1;
    }

    // map the buffers
    for(i = 0; i < NB_BUFFER; i++) {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.index = i;
        vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(vd->fd, VIDIOC_QUERYBUF, &vd->buf)) {
            log_itf(LOG_ERROR, "Unable to query buffer on device %s.", vd->device_filename);
            return -1;
        }

        vd->mem[i] = mmap(0 /* start anywhere */ ,
                          vd->buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, vd->fd,
                          vd->buf.m.offset);
        if (vd->mem[i] == MAP_FAILED) {
            log_itf(LOG_ERROR, "Unable to map buffer on device %s.", vd->device_filename);
            return -1;
        }

        DBG("Buffer mapped at address %p for device %s.", vd->mem[i], vd->device_filename);
    }

    // Queue the buffers.
    for(i = 0; i < NB_BUFFER; ++i) {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.index = i;
        vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        
        if (xioctl(vd->fd, VIDIOC_QBUF, &vd->buf) < 0) {
            log_itf(LOG_ERROR, "Unable to query buffer on device %s.", vd->device_filename);
            return -1;
        }
    }

    if (video_enable(vd)) {
        log_itf(LOG_ERROR, "Unable to enable video for device %s.", vd->device_filename);
        return -1;
    }

    return 0;
}

static int video_enable(struct video_device *vd) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    log_itf(LOG_INFO, "Starting capture on device %s.", vd->device_filename);
    ret = xioctl(vd->fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        log_itf(LOG_ERROR, "Unable to start capture on device %s", vd->device_filename);
        return ret;
    }
    vd->streaming_state = STREAMING_ON;
    return 0;
}

static int video_disable(struct video_device *vd, streaming_state disabledState) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    log_itf(LOG_INFO, "Stopping capture on device %s.", vd->device_filename);
    ret = xioctl(vd->fd, VIDIOC_STREAMOFF, &type);
    if (ret != 0) {
        log_itf(LOG_ERROR, "Unable to stop capture on device %s.", vd->device_filename);
        return ret;
    }
    log_itf(LOG_INFO, "Stopping capture done on device %s.", vd->device_filename);
    vd->streaming_state = disabledState;
    return 0;
}

int is_huffman(unsigned char *buf) {
    unsigned char *ptbuf;

    ptbuf = buf;
    while(((ptbuf[0] << 8) | ptbuf[1]) != 0xffda) {

        if (ptbuf - buf > 2048)
            return 0;

        if (((ptbuf[0] << 8) | ptbuf[1]) == 0xffc4)
            return 1;
        ptbuf++;
    }
    return 0;
}

/*
 * param dst : destination buffer
 * param dst_size : maximum size of the dst buffer
 * param src : source buffer
 * param src_size : size of the image in the src buffer
 * returns : the total size of the image
 *
 */
size_t copy_frame(unsigned char *dst, const size_t dst_size, unsigned char *src, const size_t src_size) {
    unsigned char *ptcur;
    size_t sizein, pos = 0;

    if (!is_huffman(src)) {
        // Look for where Start of Frame (Baseline DCT) goes
        for (ptcur = src; (ptcur - src) < dst_size; ptcur++) {
            if (((ptcur[0] << 8) | ptcur[1]) == 0xffc0) {
                break;
            }
        }

        // If the supplied destination buffer is too small to fit this image, fail.
        if (ptcur - src >= dst_size)
            return 0;

        sizein = ptcur - src;

        memcpy(dst, src, sizein);
        pos += sizein;

        memcpy(dst + pos, dht_data, sizeof(dht_data));
        pos += sizeof(dht_data);

        memcpy(dst + pos, ptcur, src_size - sizein);
        pos += src_size - sizein;
    } else {
        memcpy(dst, src, src_size);
        pos += src_size;
    }

    return pos;
}

size_t capture_frame(struct video_device *vd) {
    memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
    vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(vd->fd, VIDIOC_DQBUF, &vd->buf) < 0) {
        log_itf(LOG_ERROR, "Unable to dequeue buffer on device %s.", vd->device_filename);
        return -1;
    }
    
    switch(vd->format_in) {
        case V4L2_PIX_FMT_MJPEG:
            if (vd->buf.bytesused <= MIN_BYTES_USED) {
                // Prevent crash on empty image
                log_itf(LOG_WARNING, "Ignoring empty buffer on device %s.", vd->device_filename);
                return 0;
            }

            memcpy(vd->framebuffer, vd->mem[vd->buf.index], vd->buf.bytesused);
            break;

        case V4L2_PIX_FMT_YUYV:
            if (vd->buf.bytesused > vd->framebuffer_size)
                memcpy(vd->framebuffer, vd->mem[vd->buf.index], vd->framebuffer_size);
            else
                memcpy(vd->framebuffer, vd->mem[vd->buf.index], vd->buf.bytesused);
            break;

        default:
            return -1;
            break;
    }

    return vd->buf.bytesused;
}

int requeue_device_buffer(struct video_device *vd) {
    if (xioctl(vd->fd, VIDIOC_QBUF, &vd->buf) < 0) {
        log_itf(LOG_ERROR, "Unable to requeue buffer on device %s.", vd->device_filename);
        return -1;
    }

    return 0;
}

void destroy_video_device(struct video_device *vd) {
    if (vd->streaming_state == STREAMING_ON) {
        video_disable(vd, STREAMING_OFF);
    }
    
    if (CLOSE_VIDEO(vd->fd) != 0) {
        log_itf(LOG_ERROR, "Failed to close device %s.", vd->device_filename);
    }

    free(vd->framebuffer);
    vd->framebuffer = NULL;

    free(vd->device_filename);
    vd->device_filename = NULL;

    free(vd->formats);
    vd->formats = NULL;
    vd->format_count = 0;
    
    free(vd->resolutions);
    vd->resolutions = NULL;
    vd->resolution_count = 0;

    free(vd);
}


/* It should set the capture resolution
    Cheated from the openCV cap_libv4l.cpp the method is the following:
    Turn off the stream (video_disable)
    Unmap buffers
    Close the filedescriptor
    Initialize the camera again with the new resolution
*/
int set_resolution(struct video_device *vd, int width, int height) {
    int i;

    log_itf(LOG_INFO, "set_resolution(%d, %d) on device %s.", width, height, vd->device_filename);

    vd->streaming_state = STREAMING_PAUSED;
    if (video_disable(vd, STREAMING_PAUSED) != 0) {
        log_itf(LOG_ERROR, "Failed to disable streaming for device %s.", vd->device_filename);
        return -1;
    }

    DBG("Unmap buffers.");

    for(i = 0; i < NB_BUFFER; i++)
        munmap(vd->mem[i], vd->buf.length);

    if (CLOSE_VIDEO(vd->fd) != 0) {
        user_panic("Failed to close device %s.", vd->device_filename);
    }

    vd->width = width;
    vd->height = height;

    if (init_v4l2(vd) < 0) {
        user_panic("init_v4l2() failed for device %s.", vd->device_filename);
    }

    DBG("reinit done.");
    if (video_enable(vd) < 0) {
        user_panic("Failed to enable video for device %s.", vd->device_filename);
    }
    return 0;

}

