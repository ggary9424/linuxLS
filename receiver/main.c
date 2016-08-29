#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/time.h>

#include "fb_video.h"

#define CLEAR(x) (memset(&(x), 0, sizeof(x)))
#define EXEC_CMD_AND_CHECK(cmd, return_value, message) do { \
                                if ((cmd) == return_value) { \
                                    printf(#message" fail.\n"); \
                                    exit(EXIT_FAILURE); \
                                } \
                                printf(#message" exec.\n"); \
                            } while (0);

typedef struct HEADER {
    struct timeval time;
    int width;
    int height;
} Header;

static inline void calculate_fps();
void epoll_addfd(int epoll, int fd, int in);

static inline void calculate_fps() {
    static int num = 0;
    static struct timeval t1 = {0};
    static struct timeval t2 = {0};
    num++;
    if ((t1.tv_sec - t2.tv_sec) > 5) {
        float interval = (t1.tv_sec - t2.tv_sec) + (t1.tv_usec - t2.tv_usec)/1000000;
        float fps = num / (interval);
        printf("fps = %lf\n", fps);
        gettimeofday(&t1, NULL);
        gettimeofday(&t2, NULL);
        num = 0;
        return;
    }
    gettimeofday(&t1, NULL);
    if (t1.tv_sec == 0) {
        gettimeofday(&t2, NULL);
    }
    return;
}

void epoll_addfd(int epfd, int fd, int in)
{
    struct epoll_event event;
    int fsflags = fcntl(fd, F_GETFL);

    event.data.fd = fd;
    event.events = EPOLLET | EPOLLRDHUP;
    event.events |= in ? EPOLLIN : EPOLLOUT;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1)
        perror("epoll_ctl");
    /* set nonblocking fd */
    fsflags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, fsflags) == -1)
        perror("fcntl");
}

int main(void)
{
    int fbfd = -1, server_fd = -1, len = 0, flag;
    char *fb_start = NULL;
    struct sockaddr_in myaddr, clientaddr;
    socklen_t clientlen = sizeof(clientaddr);
    int buf_size = 60000;
    char buf[60003] = {0};
    char* buf_start = buf + 3; // 3 bit to keep last recv data not multiple of 4
    int remainder = 0, pic_size = 0;
    int epfd;
    struct epoll_event event;
    struct epoll_event *events;
    uint32_t evsize;
    int epoll_timeout;
    int nfds; // record epoll_wait return value
    int tmpfd; // record epoll_event.data.fd
    int cfd; // record accept return value
    int getheader = 0;
    Header header;
    struct timeval timenow;

    EXEC_CMD_AND_CHECK(fbfd = fb_open("/dev/fb0"), -1, fb_open);
    EXEC_CMD_AND_CHECK(fb_start = fb_init(fbfd), NULL, fb_init);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror ("socket failed!");
        exit(EXIT_FAILURE);
    }
    flag = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        perror("Set socket REUSEADDR");
        exit(EXIT_FAILURE);
    }
    CLEAR(myaddr);
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(8080);
    if (bind(server_fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 12) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    /* set epoll method */
    if ((epfd = epoll_create1(0)) < 0) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }
    epoll_addfd(epfd, server_fd, 1);
    evsize = 64;
    events = (struct epoll_event *)malloc(sizeof(struct epoll_event)*evsize);
    epoll_timeout = 2;
    while (1) {
        nfds = epoll_wait(epfd, events, evsize, epoll_timeout);
        for (int i = 0; i < nfds; ++i) {
            event = events[i];
            tmpfd = event.data.fd;
            if (tmpfd == server_fd) {
                while ((cfd = accept(tmpfd, (struct sockaddr*)&clientaddr, &clientlen)) > 0) {
                    epoll_addfd(epfd, cfd, 1);
                }
                if (cfd == -1) {
                    if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
                        perror("accept");
                }
            }
            else if (event.events & EPOLLIN) {
                while ((len = recv(tmpfd, buf_start, buf_size, 0)) > 0) {
                    if (!getheader) {
                        memcpy(&header, buf_start, sizeof(header));
                        getheader = 1;
                        pic_size = header.width * header.height * 2;
                        pic_size += sizeof(header);
                        gettimeofday(&timenow, NULL);
                        if (timenow.tv_sec - header.time.tv_sec < 3) {
                            if ((remainder = len%4) != 0) {
                                fb_display_pic((void *)(buf_start+sizeof(header)), fb_start, header.width, header.height, 300, 0, 0, len-sizeof(header)-remainder);
                                for (int i=0; i<remainder; i++) {
                                    *(buf-1-i) = buf[len-1-i];
                                }
                            }
                            else {
                                fb_display_pic((void *)(buf_start+sizeof(header)), fb_start, header.width, header.height, 300, 0, 0, len-sizeof(header));
                            }
                        }
                    }
                    else {
                        if (timenow.tv_sec - header.time.tv_sec < 3) {
                            int tmp = remainder;
                            if ((remainder = (len+remainder)%4) != 0) {
                                fb_display_pic((void *)(buf_start-tmp), fb_start, header.width, header.height, 300, 0, (header.width * header.height * 2 - pic_size), len+tmp-remainder);
                            }
                            else {
                                fb_display_pic((void *)(buf_start-tmp), fb_start, header.width, header.height, 300, 0, (header.width * header.height * 2 -pic_size), len+tmp);
                            }
                        }
                    }
                    pic_size -= len;
                    if (pic_size >= 60000) {
                        buf_size = 60000;
                    }
                    else if (pic_size == 0) {
                        buf_size = 60000;
                        getheader = 0;
                        calculate_fps();
                    }
                    else {
                        buf_size = pic_size;
                    }
                }
            }
            else if (event.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {

            }
        }
    }

    EXEC_CMD_AND_CHECK(fb_munmap_buf(fb_start), -1, fb_start);
    EXEC_CMD_AND_CHECK(fb_close(fbfd), -1, fb_close);

    return 0;
}
