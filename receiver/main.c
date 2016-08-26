#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "fb_video.h"

#define CLEAR(x) (memset(&(x), 0, sizeof(x)))
#define EXEC_CMD_AND_CHECK(cmd, return_value, message) do { \
                                if ((cmd) == return_value) { \
                                    printf(#message" fail.\n"); \
                                    exit(EXIT_FAILURE); \
                                } \
                                printf(#message" exec.\n"); \
                            } while (0);

int main(void)
{
    int fbfd = -1, server_fd = -1, len = 0;
    char *fb_start = NULL;
    struct sockaddr_in myaddr, fromaddr;
    socklen_t addr_len;
    char buf[65504] = {0};

    EXEC_CMD_AND_CHECK(fbfd = fb_open("/dev/fb0"), -1, fb_open);
    EXEC_CMD_AND_CHECK(fb_start = fb_init(fbfd), NULL, fb_init);

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror ("socket failed!");
        exit(1);
    }
    CLEAR(myaddr);
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(8080);
    if (bind(server_fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    CLEAR(fromaddr);
    addr_len = sizeof(fromaddr);
    while (1) {
        len = recvfrom(server_fd, buf, 65504, 0, (struct sockaddr*)&fromaddr, &addr_len);
        if (len > 0) {
            if(len == 65504)
                fb_display_pic((void *)buf, fb_start, 320, 180, 0, 0, 0, 65504);
            else
                fb_display_pic((void *)buf, fb_start, 320, 180, 0, 0, 65504, 49696);
        } else {
            break;
        }
    }

    EXEC_CMD_AND_CHECK(fb_munmap_buf(fb_start), -1, fb_start);
    EXEC_CMD_AND_CHECK(fb_close(fbfd), -1, fb_close);

    return 0;
}
