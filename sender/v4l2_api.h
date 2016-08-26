#ifndef V4L2_API_H
#define V4L2_API_H

typedef struct my_buffer {
    void *start;
    size_t length;
} my_buffer;

/* return open fd */
int v4l2_open_dev(char *video);
int v4l2_init_dev(int fd, int *req_buffer_num, my_buffer **bufs, int *width, int *height);
int v4l2_start_capstream(int fd, int req_buffer_num);
void *v4l2_getpic(int fd, my_buffer *bufs);
int v4l2_stop_capstream(int fd);
int v4l2_munmap_bufs(int req_buffer_num, my_buffer **bufs);
/* return 0 success, return -1 fail */
int v4l2_close_dev(int fd);

#endif
