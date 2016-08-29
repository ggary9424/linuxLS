#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "fb_video.h"

#define CLEAR(x) (memset(&(x), 0, sizeof(x)))

typedef struct RGB {
    int r;
    int g;
    int b;
} RGB;

static inline int clip(int value, int min, int max);
static inline int xioctl(int fd, int request, void *arg);
static inline RGB YUV_to_RGB(int y, int u, int v);

static struct fb_fix_screeninfo finfo = {0};
static struct fb_var_screeninfo vinfo = {0};

static inline int clip(int value, int min, int max)
{
    return (value > max ? max : (value < min ? min : value));
}

static inline RGB YUV_to_RGB(int y, int u, int v)
{
    RGB rgb;
    int r,g,b;
    u = u -128;
    v = v-128;
    r = (298 * y + 409 * v + 128) >> 8;
    g = (298 * y - 100 * u - 208 * v + 128) >> 8;
    b = (298 * y + 516 * u + 128) >> 8;
    rgb.r = clip(r, 0, 255);
    rgb.b = clip(b, 0, 255);
    rgb.g = clip(g, 0, 255);
    return rgb;
}

static inline int xioctl(int fd, int request, void *arg)
{
    int r;
    /* Here use this method to make sure cmd success */
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && EINTR == errno);
    return r;
}

int fb_open(char *dev_name)
{
    int fbfd = open(dev_name, O_RDWR);
    if (fbfd==-1) {
        perror("open framebuffer device");
        return -1;
    }
    return fbfd;
}

char* fb_init(int fbfd)
{
    char *fb_start;
    int screen_bytes;

    /* Get fixed screen information */
    if (xioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("FBIOGET_FSCREENINFO");
        return NULL;
    }
    printf("line_length: %d\n", finfo.line_length);
    /* Get variable screen information */
    if (xioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("FBIOGET_VSCREENINFO");
        return NULL;
    }
    screen_bytes = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    fb_start = (char *)mmap(NULL, screen_bytes, PROT_READ|PROT_WRITE,MAP_SHARED , fbfd, 0);
    if ((long)fb_start == -1) {
        perror("mmap framebuffer");
    }
    return fb_start;
}

void fb_display_pic(void *pic, char *fb_start, int width, int height,
                    int x_offset, int y_offset, int start_byte, int pic_len)
{
    if (!pic_len)
        return;
    unsigned char* in = (unsigned char*)pic;
    int y0,y1,u,v;
    RGB rgb;
    long location=0;
    int i = 0;
    int y, x;
    for (y=(start_byte/(width*2)); y<height; y++) {
        if (i == 0)
            x = (start_byte/2)%width;
        else
            x = 0;
        for(; x<width; x+=2) {
            location = (x+x_offset+vinfo.xoffset) * 4 +
                       (y+y_offset+vinfo.yoffset) * finfo.line_length;
            y0 = in[i];
            y1 = in[i+2];
            u = in[i+1];
            v = in[i+3];

            rgb = YUV_to_RGB(y0, u, v);
            fb_start[location+0] = clip(rgb.b, 0, 255);
            fb_start[location+1] = clip(rgb.g, 0, 255);
            fb_start[location+2] = clip(rgb.r, 0, 255);
            fb_start[location+3] = 255;

            rgb = YUV_to_RGB(y1, u, v);
            fb_start[location+4] = clip(rgb.b, 0, 255);
            fb_start[location+5] = clip(rgb.g, 0, 255);
            fb_start[location+6] = clip(rgb.r, 0, 255);
            fb_start[location+7] = 255;
            i += 4;
            if (i == pic_len)
                return ;
        }
    }
    return ;
}

int fb_munmap_buf(char *fb_start)
{
    int screen_bytes = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    if (munmap(fb_start, screen_bytes) == -1) {
        perror("munmap framebuffer device");
        return -1;
    }
    return 1;
}

int fb_close(int fd)
{
    if (close(fd) == -1) {
        perror("close framebuffer dev");
        return -1;
    }
    return 0;
}
