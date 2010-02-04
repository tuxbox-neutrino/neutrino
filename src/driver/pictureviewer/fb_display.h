#ifndef __pictureviewer_fbdisplay__
#define __pictureviewer_fbdisplay__
void* convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp = 0xFF);
extern void fb_display(unsigned char *rgbbuff, int x_size, int y_size, int x_pan, int y_pan, int x_offs, int y_offs, bool clearfb = true, int transp = 0xFF);
extern void getCurrentRes(int *x,int *y);
unsigned char * make15color_errdiff(unsigned char * orgin,int x, int y);
#endif

