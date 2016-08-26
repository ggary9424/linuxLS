#ifndef FB_VIDEO_H
#define FB_VIDEO_H

/* fb means framebuffer, we play the video through this dev */

/* return open fd */
int fb_open(char *dev_name);
/* return start pointer we map */
char *fb_init(int fbfd);
void fb_display_pic(void *pic, char *fb_start, int width, int height,
                    int x_offset, int y_offset, int start_byte, int pic_len);
int fb_munmap_buf(char *fb_start);
/* return 0 success, return -1 fail */
int fb_close(int fd);

#endif