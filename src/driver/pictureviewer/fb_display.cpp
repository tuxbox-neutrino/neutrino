/* FrameBuffer Image Display Function
 * (c) smoku/2000 
 */

#include <stdint.h>
#include <linux/fb.h>
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <string.h>
#include <errno.h>
#include "fb_display.h"
#include "pictureviewer.h"
#include "driver/framebuffer.h"

static unsigned char *lfb = 0;

void blit2FB(void *fbbuff,
	unsigned int pic_xs, unsigned int pic_ys,
	unsigned int scr_xs, unsigned int scr_ys,
	unsigned int xp, unsigned int yp,
	unsigned int xoffs, unsigned int yoffs);
void fb_display(unsigned char *rgbbuff, int x_size, int y_size, int x_pan, int y_pan, int x_offs, int y_offs, bool clearfb, int transp)
{
	struct fb_var_screeninfo *var;
	unsigned short *fbbuff = NULL;

	if(rgbbuff == NULL)
		return;

	/* read current video mode */
	var = CFrameBuffer::getInstance()->getScreenInfo();
	lfb = (unsigned char *)CFrameBuffer::getInstance()->getFrameBufferPointer();

	/* correct panning */
	if(x_pan > x_size - (int)var->xres) x_pan = 0;
	if(y_pan > y_size - (int)var->yres) y_pan = 0;
	/* correct offset */
	if(x_offs + x_size > (int)var->xres) x_offs = 0;
	if(y_offs + y_size > (int)var->yres) y_offs = 0;

	/* blit buffer 2 fb */
	fbbuff = (unsigned short *) convertRGB2FB(rgbbuff, x_size, y_size, transp);
	if(fbbuff==NULL)
		return;

	/* ClearFB if image is smaller */
	/* if(x_size < (int)var->xres || y_size < (int)var->yres) */
	if(clearfb)
		CFrameBuffer::getInstance()->Clear();

	blit2FB(fbbuff, x_size, y_size, var->xres, var->yres, x_pan, y_pan, x_offs, y_offs);
	free(fbbuff);
}

void blit2FB(void *fbbuff,
	unsigned int pic_xs, unsigned int pic_ys,
	unsigned int scr_xs, unsigned int scr_ys,
	unsigned int xp, unsigned int yp,
	unsigned int xoffs, unsigned int yoffs)
{
	int i, xc, yc;
	unsigned char *cp; unsigned short *sp; unsigned int *ip;
	ip = (unsigned int *) fbbuff;
	sp = (unsigned short *) ip;
	cp = (unsigned char *) sp;

	xc = (pic_xs > scr_xs) ? scr_xs : pic_xs;
	yc = (pic_ys > scr_ys) ? scr_ys : pic_ys;

	unsigned int stride = CFrameBuffer::getInstance()->getStride();

	for(i = 0; i < yc; i++){
		memmove(lfb+(i+yoffs)*stride+xoffs*4, ip + (i+yp)*pic_xs+xp, xc*4);
	}
}

void* convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int transp)
{
	unsigned long i;
	unsigned int *fbbuff;
	unsigned long count = x*y;

	fbbuff = (unsigned int *) malloc(count * sizeof(unsigned int));
	if(fbbuff == NULL)
	{
		printf("convertRGB2FB: Error: malloc\n");
		return NULL;
	}

	for(i = 0; i < count ; i++)
		fbbuff[i] = (transp << 24) | ((rgbbuff[i*3] << 16) & 0xFF0000) | ((rgbbuff[i*3+1] << 8) & 0xFF00) | (rgbbuff[i*3+2] & 0xFF);

	return (void *) fbbuff;
}
