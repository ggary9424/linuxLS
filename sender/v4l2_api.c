#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/videodev2.h>

#include "v4l2_api.h"

#define CLEAR(x) (memset(&(x), 0, sizeof(x)))

static int xioctl(int fd, int request, void *arg);

static int xioctl(int fd, int request, void *arg)
{
    int r;
    /* Here use this method to make sure cmd success */
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && EINTR == errno);
    return r;
}

int v4l2_open_dev(char *video)
{
    struct stat st;
    int fd;

    if (stat(video, &st) == -1) {
        perror("stat");
        return -1;
    }
    /*
     * S_ISCHR checks the file mode to see whether the file is a character device file.
     * If so it returns True.
     */
    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "%s is no device\n", video);
        return -1;
    }
    if ((fd = open(video, O_RDWR|O_NONBLOCK, 0)) == -1) {
        fprintf(stderr, "Cannot open '%s'\n", video);
        return -1;
    }
    return fd;
}

static int init_mmap(int fd, int *req_buffer_num, my_buffer **bufs)
{
    struct v4l2_requestbuffers req;

    req.count = *req_buffer_num;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    /* VIDIOC_REQBUFS: request buffers */
    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "device does not support memory mapping\n");
            return -1;
        } else {
            perror("VIDIOC_REQBUFS");
            return -1;
        }
    }
    if (req.count < *req_buffer_num) {
        if (req.count > 0) {
            fprintf(stderr, "just request %d buffers memory on device\n", *req_buffer_num);
            *req_buffer_num = req.count;
        } else {
            fprintf(stderr, "insufficient buffer memory on device\n");
            return -1;
        }
    }
    *bufs = (my_buffer *)calloc(req.count, sizeof(my_buffer));
    if (!(*bufs)) {
        fprintf(stderr, "Out of memory\n");
        return -1;
    }
    for (int i=0; i<req.count; i++) {
        struct v4l2_buffer v4l2_buf;

        CLEAR(v4l2_buf);
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = V4L2_MEMORY_MMAP;
        v4l2_buf.index = i;
        /* VIDIOC_QUERYBUF: get mmap information we need */
        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &v4l2_buf)) {
            perror("VIDIOC_QUERYBUF");
            return -1;
        }
        (*bufs+i)->length = v4l2_buf.length;
        (*bufs+i)->start = mmap(NULL, v4l2_buf.length, PROT_READ|PROT_WRITE
                                , MAP_SHARED, fd, v4l2_buf.m.offset);
        if ((*bufs+i)->start == MAP_FAILED) {
            perror("mmap");
            return -1;
        }
    }
    return 1;
}

int v4l2_init_dev(int fd, int *req_buffer_num, my_buffer **bufs, int *width, int *height)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;

    /*
     *struct v4l2_capability {
     *     __u8    driver[16];     driver name
     *     __u8    card[32];       i.e. "Hauppauge WinTV"
     *     __u8    bus_info[32];   i.e. "PCI:" + pci_name(pci_dev)
     *     __u32   version;        should use KERNEL_VERSION()
     *     __u32   capabilities;   device capabilities
     *     __u32   reserved[4];
     * };
     * VIDIOC_QUERYCAP: query capabilities
     */
    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        /* EINVAL: invalid argument */
        if (EINVAL == errno) {
            fprintf(stderr, "the device is no V4L2 device\n");
            return -1;
        } else {
            return -1;
        }
    }
    /* Check if it is a video capture device */
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "the device is no video capture device\n");
        return -1;
    }
    /* Check if it supports streaming I/O ioctls */
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "the device does not support streaming i/o\n");
        return -1;
    }
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = *width;
    fmt.fmt.pix.height = *height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    /*
     * use VIDIOC_S_FMT to set video format,
     * but we need to use VIDIOC_TRY_FMT to check real format
     */
    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("VIDIOC_S_FMT");
        return -1;
    }
    if (xioctl(fd, VIDIOC_TRY_FMT, &fmt) == -1) {
        perror("VIDIOC_TRY_FMT");
        return -1;
    }
    *width = fmt.fmt.pix.width;
    *height = fmt.fmt.pix.height;
    if (init_mmap(fd, req_buffer_num, bufs) == -1) {
        fprintf(stderr, "init_mmap fail\n");
        return -1;
    }
    return 1;
}

int v4l2_start_capstream(int fd, int req_buffer_num)
{
    enum v4l2_buf_type type;

    for (int i=0; i<req_buffer_num; ++i) {
        struct v4l2_buffer buf;
        CLEAR (buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        /* VIDIOC_QBU */
        if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
            perror("VIDIOC_QBUF");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    /* VIDIOC_STREAMON: video stream on */
    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("VIDIOC_STREAMON");
        return -1;
    }
    return 1;
}

void *v4l2_getpic(int fd, my_buffer *bufs)
{
    struct v4l2_buffer v4l2_buf;

    CLEAR (v4l2_buf);
    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;
    while (1) {
        /* VIDIOC_DQBUF */
        if (xioctl (fd, VIDIOC_DQBUF, &v4l2_buf) == -1) {
            switch (errno) {
                case EAGAIN:
                    continue;
                case EIO:
                default:
                    perror("VIDIOC_DQBUF");
                    return NULL;
            }
        }
        break;
    }
    /* VIDIOC_QBUF */
    if (-1 == xioctl (fd, VIDIOC_QBUF, &v4l2_buf)) {
        perror("VIDIOC_QBUF");
        return NULL;
    }
    return (bufs[v4l2_buf.index].start);
}

int v4l2_stop_capstream(int fd)
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    /* VIDIOC_STREAMOFF */
    if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("VIDIOC_STREAMOFF");
        return -1;
    }
    return 1;
}

int v4l2_munmap_bufs(int req_buffer_num, my_buffer **bufs)
{
    for (int i=0; i<req_buffer_num; i++) {
        if (munmap((*bufs+i)->start, (*bufs+i)->length) == -1) {
            perror("munmap");
            return -1;
        }
    }
    free(*bufs);
    return 1;
}

int v4l2_close_dev(int fd)
{
    if (close(fd) == -1) {
        perror("close video dev");
        return -1;
    }
    return 0;
}
