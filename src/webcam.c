/*
 *  V4L2 video capture example
 *	http://linuxtv.org/downloads/v4l-dvb-apis/capture-example.html
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h> /* low-level i/o */
#include <linux/videodev2.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "httpd.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

enum v_io_method {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

struct v_buffer {
    void *start;
    size_t length;
};

static char v_dev_name[32];
static enum v_io_method v_io = IO_METHOD_MMAP;
static int v_fd = -1;
struct v_buffer *v_buffers;
static unsigned int n_buffers = 0;
static int v_force_format = 0;
static int v_last_working_video_source = -1;
static int v_last_working_video_format = 0;

// ------------------------- memory manager ------------------------

#define MM_BUF_SIZE 64
static int mm_index;
static void *mm_buf[MM_BUF_SIZE];

// initialize the memory pointer buffer
static void mem_manager_begin(void) {
    int i;
    for (i = 0; i < MM_BUF_SIZE; i++)
        mm_buf[i] = NULL;
    mm_index = 0;
}

// allocate memory and keep track of the used pointers
static void *mem_manager_calloc(size_t elements, size_t size) {
    void *ptr = calloc(elements, size);
    if (mm_index < MM_BUF_SIZE) {
        mm_buf[mm_index++] = ptr;
    }
    return ptr;
}

// release all used memory pointers
static void mem_manager_end(void) {
    int i;
    for (i = 0; i < MM_BUF_SIZE; i++) {
        if (mm_buf[i])
            free(mm_buf[i]);
    }
    return;
}

// ------------------------- memory manager ------------------------

// YUY2 to RGB conversion function
void YUY2_to_RGB(const uint8_t *yuy2_data, uint8_t *rgb_data, int width, int height) {
    int i, j;
    int y0, u, y1, v;
    int r, g, b;

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j += 2) {
            y0 = yuy2_data[i * width * 2 + j * 2];
            u = yuy2_data[i * width * 2 + j * 2 + 1] - 128;
            y1 = yuy2_data[i * width * 2 + j * 2 + 2];
            v = yuy2_data[i * width * 2 + j * 2 + 3] - 128;

            // Convert YUV to RGB
            r = (298 * y0 + 409 * v + 128) >> 8;
            g = (298 * y0 - 100 * u - 208 * v + 128) >> 8;
            b = (298 * y0 + 516 * u + 128) >> 8;

            // Ensure RGB values are within range
            r = r > 255 ? 255 : (r < 0 ? 0 : r);
            g = g > 255 ? 255 : (g < 0 ? 0 : g);
            b = b > 255 ? 255 : (b < 0 ? 0 : b);

            rgb_data[i * width * 3 + j * 3] = b;      // r
            rgb_data[i * width * 3 + j * 3 + 1] = g;  // g
            rgb_data[i * width * 3 + j * 3 + 2] = r;  // b

            r = (298 * y1 + 409 * v + 128) >> 8;
            g = (298 * y1 - 100 * u - 208 * v + 128) >> 8;
            b = (298 * y1 + 516 * u + 128) >> 8;

            r = r > 255 ? 255 : (r < 0 ? 0 : r);
            g = g > 255 ? 255 : (g < 0 ? 0 : g);
            b = b > 255 ? 255 : (b < 0 ? 0 : b);

            rgb_data[i * width * 3 + j * 3 + 3] = b;  // r
            rgb_data[i * width * 3 + j * 3 + 4] = g;  // g
            rgb_data[i * width * 3 + j * 3 + 5] = r;  // b
        }
    }
}

static void v_errno_print(const char *s) {
    if (debug)
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
}

static int v_xioctl(int fh, int request, void *arg) {
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

// fixed bitmap header for 640 x 480 RGB 24-bit color per point
const uint8_t bmp_header[54] = {
    0x42, 0x4D, 0x36, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x80, 0x02, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// export the result to a file
// auto select between the original compression and YUY2/YUYV to RGB for raw format cameras
static int v_process_image(const char *filename, const void *p, int size) {
    FILE *ofptr;
    ofptr = fopen(filename, "wb");
    if (ofptr) {
        if (size == 614400) {
            // most likely YUYV or YUY2 raw camera formats
            uint8_t *rgb_data = (uint8_t *)mem_manager_calloc(1, 921600);
            if (rgb_data) {
                // Convert YUY2 to RGB
                YUY2_to_RGB(p, rgb_data, 640, 480);
                int padded_size = (640 * 3 + 3) & ~3;
                int file_size = 54 + padded_size * 480;
                int zero_padding = padded_size - 640 * 3;
                fwrite(bmp_header, 54, 1, ofptr);
                for (int i = 480 - 1; i >= 0; i--) {
                    fwrite(rgb_data + i * 640 * 3, 1, 640 * 3, ofptr);
                    fseek(ofptr, zero_padding, SEEK_CUR);
                }
            } else {
                fwrite(p, size, 1, ofptr);
            }
        } else {
            // maybe already compressed as AVI
            fwrite(p, size, 1, ofptr);
        }
        fclose(ofptr);
        return 0;
    }
    return -1;
}

static int v_read_frame(const char *filename) {
    struct v4l2_buffer buf;
    unsigned int i;

    switch (v_io) {
        case IO_METHOD_READ:
            if (-1 == read(v_fd, v_buffers[0].start, v_buffers[0].length)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        v_errno_print("read");
                        return -1;
                }
            }

            if (v_process_image(filename, v_buffers[0].start, v_buffers[0].length) < 0)
                return -1;
            break;

        case IO_METHOD_MMAP:
            CLEAR(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (-1 == v_xioctl(v_fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        v_errno_print("VIDIOC_DQBUF");
                        return -1;
                }
            }

            assert(buf.index < n_buffers);

            if (v_process_image(filename, v_buffers[buf.index].start, buf.bytesused) < 0)
                return -1;

            if (-1 == v_xioctl(v_fd, VIDIOC_QBUF, &buf)) {
                v_errno_print("VIDIOC_QBUF");
                return -1;
            }
            break;

        case IO_METHOD_USERPTR:
            CLEAR(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;

            if (-1 == v_xioctl(v_fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        v_errno_print("VIDIOC_DQBUF");
                        return -1;
                }
            }

            for (i = 0; i < n_buffers; ++i)
                if (buf.m.userptr == (unsigned long)v_buffers[i].start && buf.length == v_buffers[i].length)
                    break;

            assert(i < n_buffers);

            if (v_process_image(filename, (void *)buf.m.userptr, buf.bytesused) < 0)
                return -1;

            if (-1 == v_xioctl(v_fd, VIDIOC_QBUF, &buf)) {
                v_errno_print("VIDIOC_QBUF");
                return -1;
            }
            break;
    }

    return 1;
}

static int v_mainloop(const char *filename, int v_frame_count) {
    unsigned int count;

    count = v_frame_count;

    while (count-- > 0) {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO(&fds);
            FD_SET(v_fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select(v_fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if (EINTR == errno)
                    continue;
                v_errno_print("select");
                return -1;
            }

            if (0 == r) {
                if (debug)
                    fprintf(stderr, "select timeout\n");
                return -1;
            }

            r = v_read_frame(filename);
            if (r < 0)
                return -1;
            if (r)
                break;
            /* EAGAIN - continue select loop. */
        }
    }
    return 0;
}

static int v_stop_capturing(void) {
    enum v4l2_buf_type type;

    switch (v_io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == v_xioctl(v_fd, VIDIOC_STREAMOFF, &type)) {
                v_errno_print("VIDIOC_STREAMOFF");
                return -1;
            }
            break;
    }
    return 0;
}

static int v_start_capturing(void) {
    unsigned int i;
    enum v4l2_buf_type type;

    switch (v_io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (-1 == v_xioctl(v_fd, VIDIOC_QBUF, &buf)) {
                    v_errno_print("VIDIOC_QBUF");
                    return -1;
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == v_xioctl(v_fd, VIDIOC_STREAMON, &type)) {
                v_errno_print("VIDIOC_STREAMON");
                return -1;
            }
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;
                buf.index = i;
                buf.m.userptr = (unsigned long)v_buffers[i].start;
                buf.length = v_buffers[i].length;

                if (-1 == v_xioctl(v_fd, VIDIOC_QBUF, &buf)) {
                    v_errno_print("VIDIOC_QBUF");
                    return -1;
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == v_xioctl(v_fd, VIDIOC_STREAMON, &type)) {
                v_errno_print("VIDIOC_STREAMON");
                return -1;
            }
            break;
    }
    return 0;
}

static int v_uninit_device(void) {
    unsigned int i;

    switch (v_io) {
        case IO_METHOD_READ:
            // free(v_buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap(v_buffers[i].start, v_buffers[i].length)) {
                    v_errno_print("munmap");
                    return -1;
                }
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i)
                // free(v_buffers[i].start);
                break;
    }

    // free(v_buffers);
    return 0;
}

static int v_init_read(unsigned int v_buffer_size) {
    v_buffers = mem_manager_calloc(1, sizeof(*v_buffers));

    if (!v_buffers) {
        if (debug)
            fprintf(stderr, "Out of memory\n");
        return -1;
    }

    v_buffers[0].length = v_buffer_size;
    v_buffers[0].start = mem_manager_calloc(1, v_buffer_size);

    if (!v_buffers[0].start) {
        if (debug)
            fprintf(stderr, "Out of memory\n");
        return -1;
    }
    return 0;
}

static int v_init_mmap(void) {
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == v_xioctl(v_fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            if (debug)
                fprintf(stderr,
                        "%s does not support "
                        "memory mapping\n",
                        v_dev_name);
            return -1;
        } else {
            v_errno_print("VIDIOC_REQBUFS");
            return -1;
        }
    }

    if (req.count < 2) {
        if (debug)
            fprintf(stderr, "Insufficient buffer memory on %s\n",
                    v_dev_name);
        return -1;
    }

    v_buffers = mem_manager_calloc(req.count, sizeof(*v_buffers));

    if (!v_buffers) {
        if (debug)
            fprintf(stderr, "Out of memory\n");
        return -1;
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (-1 == v_xioctl(v_fd, VIDIOC_QUERYBUF, &buf)) {
            v_errno_print("VIDIOC_QUERYBUF");
            return -1;
        }

        v_buffers[n_buffers].length = buf.length;
        v_buffers[n_buffers].start =
            mmap(NULL /* start anywhere */,
                 buf.length,
                 PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */,
                 v_fd, buf.m.offset);

        if (MAP_FAILED == v_buffers[n_buffers].start) {
            v_errno_print("mmap");
            return -1;
        }
    }
    return 0;
}

static int v_init_userp(unsigned int v_buffer_size) {
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if (-1 == v_xioctl(v_fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            if (debug)
                fprintf(stderr,
                        "%s does not support "
                        "user pointer i/o\n",
                        v_dev_name);
            return -1;
        } else {
            v_errno_print("VIDIOC_REQBUFS");
            return -1;
        }
    }

    v_buffers = mem_manager_calloc(4, sizeof(*v_buffers));

    if (!v_buffers) {
        if (debug)
            fprintf(stderr, "Out of memory\n");
        return -1;
    }

    for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
        v_buffers[n_buffers].length = v_buffer_size;
        v_buffers[n_buffers].start = mem_manager_calloc(1, v_buffer_size);

        if (!v_buffers[n_buffers].start) {
            if (debug)
                fprintf(stderr, "Out of memory\n");
            return -1;
        }
    }
    return 0;
}

static int v_init_device(void) {
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (-1 == v_xioctl(v_fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            if (debug)
                fprintf(stderr, "%s is no V4L2 device\n", v_dev_name);
            return -1;
        } else {
            v_errno_print("VIDIOC_QUERYCAP");
            return -1;
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        if (debug)
            fprintf(stderr, "%s is no video capture device\n", v_dev_name);
        return -1;
    }

    switch (v_io) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                if (debug)
                    fprintf(stderr, "%s does not support read i/o\n", v_dev_name);
                return -1;
            }
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                if (debug)
                    fprintf(stderr, "%s does not support streaming i/o\n", v_dev_name);
                return -1;
            }
            break;
    }

    /* Select video input, video standard and tune here. */

    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == v_xioctl(v_fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == v_xioctl(v_fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    } else {
        /* Errors ignored. */
    }

    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v_force_format) {
        fmt.fmt.pix.width = 640;
        fmt.fmt.pix.height = 480;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

        if (-1 == v_xioctl(v_fd, VIDIOC_S_FMT, &fmt)) {
            v_errno_print("VIDIOC_S_FMT");
            return -1;
        }

        /* Note VIDIOC_S_FMT may change width and height. */
    } else {
        /* Preserve original settings as set by v4l2-ctl for example */
        if (-1 == v_xioctl(v_fd, VIDIOC_G_FMT, &fmt)) {
            v_errno_print("VIDIOC_G_FMT");
            return -1;
        }
    }

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    switch (v_io) {
        case IO_METHOD_READ:
            if (v_init_read(fmt.fmt.pix.sizeimage) < 0)
                return -1;
            break;

        case IO_METHOD_MMAP:
            if (v_init_mmap() < 0)
                return -1;
            break;

        case IO_METHOD_USERPTR:
            if (v_init_userp(fmt.fmt.pix.sizeimage) < 0)
                return -1;
            break;
    }
    return 0;
}

static int v_close_device(void) {
    if (-1 == close(v_fd)) {
        v_errno_print("close");
        return -1;
    }

    v_fd = -1;
    return 0;
}

static int v_open_device(void) {
    struct stat st;

    if (-1 == stat(v_dev_name, &st)) {
        if (debug)
            fprintf(stderr, "Cannot identify '%s': %d, %s\n", v_dev_name, errno, strerror(errno));
        return -1;
    }

    if (!S_ISCHR(st.st_mode)) {
        if (debug)
            fprintf(stderr, "%s is no device\n", v_dev_name);
        return -1;
    }

    v_fd = open(v_dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == v_fd) {
        if (debug)
            fprintf(stderr, "Cannot open '%s': %d, %s\n", v_dev_name, errno, strerror(errno));
        return -1;
    }
    return 0;
}
/*
static void usage(FILE *fp, int argc, char **argv)
{
        fprintf(fp,
                 "Usage: %s [options]\n\n"
                 "Version 1.3\n"
                 "Options:\n"
                 "-d | --device name   Video device name [%s]\n"
                 "-h | --help          Print this message\n"
                 "-m | --mmap          Use memory mapped buffers [default]\n"
                 "-r | --read          Use read() calls\n"
                 "-u | --userp         Use application allocated buffers\n"
                 "-o | --output        Outputs stream to stdout\n"
                 "-f | --format        Force format to 640x480 YUYV\n"
                 "-c | --count         Number of frames to grab [%i]\n"
                 "",
                 argv[0], v_dev_name, v_frame_count);
}
*/

// main entry point to capture one image frame
// v_frame_count - the number of frames requested
// return 1 if the file v_filename was created
// return 0 in case of error, the external call should create a default image
int v_capture_image(const char *v_filename, int v_frame_count) {
    int result = 0;
    int i, j, beg, end, fbeg, fend;
    if (v_last_working_video_source >= 0) {
        // good video source is already available
        beg = v_last_working_video_source;
        end = v_last_working_video_source;
        fbeg = v_last_working_video_format;
        fend = v_last_working_video_format;
    } else {
        // try to find another video source and format
        beg = 0;
        end = 3;
        fbeg = 0;
        fend = 1;
    }
    for (j = fbeg; j <= fend; j++) {
        v_force_format = j;
        for (i = beg; i <= end; i++) {
            sprintf(v_dev_name, "/dev/video%d", i);
            mem_manager_begin();
            if (v_open_device() >= 0) {
                if (v_init_device() >= 0) {
                    if (v_start_capturing() >= 0) {
                        if (v_mainloop(v_filename, v_frame_count) >= 0) {
                            if (v_stop_capturing() >= 0) {
                                if (v_uninit_device() >= 0) {
                                    if (v_close_device() >= 0) {
                                        result = 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            mem_manager_end();
            if (result) {
                // found a video source with no errors
                v_last_working_video_source = i;
                v_last_working_video_format = j;
                goto e_n_d;
            }
        }
    }
e_n_d:
    if (!result) {
        // missing a good video source, set mode searching for the next call
        v_last_working_video_source = -1;
        v_last_working_video_format = 0;
    }
    return result;
}
