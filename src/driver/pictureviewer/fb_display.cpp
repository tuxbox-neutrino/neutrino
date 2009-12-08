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
/*
 * FrameBuffer Image Display Function
 * (c) smoku/2000
 *
 */

/* Public Use Functions:
 *
 * extern void fb_display(unsigned char *rgbbuff,
 *     int x_size, int y_size,
 *     int x_pan, int y_pan,
 *     int x_offs, int y_offs);
 *
 * extern void getCurrentRes(int *x,int *y);
 *
 */

static unsigned short red[256], green[256], blue[256];
static struct fb_cmap map332 = {0, 256, red, green, blue, NULL};
static unsigned short red_b[256], green_b[256], blue_b[256];
static struct fb_cmap map_back = {0, 256, red_b, green_b, blue_b, NULL};

static unsigned char *lfb = 0;

int openFB(const char *name);
//void closeFB(int fh);
//void getVarScreenInfo(int fh, struct fb_var_screeninfo *var);
//void setVarScreenInfo(int fh, struct fb_var_screeninfo *var);
void getFixScreenInfo(struct fb_fix_screeninfo *fix);
void set332map();
void blit2FB(void *fbbuff,
	unsigned int pic_xs, unsigned int pic_ys,
	unsigned int scr_xs, unsigned int scr_ys,
	unsigned int xp, unsigned int yp,
	unsigned int xoffs, unsigned int yoffs,
	int cpp);
void clearFB(int bpp,int cpp);
inline unsigned short make16color(uint32_t  r, uint32_t  g, 
											 uint32_t  b, uint32_t  rl, 
											 uint32_t  ro, uint32_t  gl, 
											 uint32_t  go, uint32_t  bl, 
											 uint32_t  bo, uint32_t  tl, 
											 uint32_t  to);

void fb_display(unsigned char *rgbbuff, int x_size, int y_size, int x_pan, int y_pan, int x_offs, int y_offs, bool clearfb, int transp)
{
    struct fb_var_screeninfo *var;
    unsigned short *fbbuff = NULL;
    int bp = 0;
    if(rgbbuff==NULL)
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
    
//printf("fb_display: bits_per_pixel: %d\n", var->bits_per_pixel);
//printf("fb_display: var->xres %d var->yres %d x_size %d y_size %d\n", var->xres, var->yres, x_size, y_size);
    /* blit buffer 2 fb */
    fbbuff = (unsigned short *) convertRGB2FB(rgbbuff, x_size, y_size, var->bits_per_pixel, &bp, transp);
    if(fbbuff==NULL)
		 return;
	 /* ClearFB if image is smaller */
    //if(x_size < (int)var->xres || y_size < (int)var->yres)
    if(clearfb)
       clearFB(var->bits_per_pixel, bp);
    blit2FB(fbbuff, x_size, y_size, var->xres, var->yres, x_pan, y_pan, x_offs, y_offs, bp);
    free(fbbuff);
}

void getCurrentRes(int *x, int *y)
{
    struct fb_var_screeninfo *var;
    var = CFrameBuffer::getInstance()->getScreenInfo();
    *x = var->xres;
    *y = var->yres;
//printf("getCurrentRes: %dx%d\n", var->xres, var->yres);
}

void make332map(struct fb_cmap *map)
{
	int rs, gs, bs, i;
	int r = 8, g = 8, b = 4;

	map->red = red;
	map->green = green;
	map->blue = blue;

	rs = 256 / (r - 1);
	gs = 256 / (g - 1);
	bs = 256 / (b - 1);
	
	for (i = 0; i < 256; i++) {
		map->red[i]   = (rs * ((i / (g * b)) % r)) * 255;
		map->green[i] = (gs * ((i / b) % g)) * 255;
		map->blue[i]  = (bs * ((i) % b)) * 255;
	}
}
/*
void set8map(int fh, struct fb_cmap *map)
{
    if (ioctl(fh, FBIOPUTCMAP, map) < 0) {
        fprintf(stderr, "Error putting colormap");
        exit(1);
    }
}

void get8map(int fh, struct fb_cmap *map)
{
    if (ioctl(fh, FBIOGETCMAP, map) < 0) {
        fprintf(stderr, "Error getting colormap");
        exit(1);
    }
}
*/
void set332map()
{
    make332map(&map332);
    CFrameBuffer::getInstance()->paletteSet(&map332);
}

void blit2FB(void *fbbuff,
	unsigned int pic_xs, unsigned int pic_ys,
	unsigned int scr_xs, unsigned int scr_ys,
	unsigned int xp, unsigned int yp,
	unsigned int xoffs, unsigned int yoffs,
	int cpp)
{
    int i, xc, yc;
    unsigned char *cp; unsigned short *sp; unsigned int *ip;
    ip = (unsigned int *) fbbuff;
    sp = (unsigned short *) ip;
    cp = (unsigned char *) sp;

    xc = (pic_xs > scr_xs) ? scr_xs : pic_xs;
    yc = (pic_ys > scr_ys) ? scr_ys : pic_ys;
    
    unsigned int stride = CFrameBuffer::getInstance()->getStride();

	 switch(cpp){
		 case 1:
	    set332map();
	    for(i = 0; i < yc; i++){
			 memcpy(lfb+(i+yoffs)*stride+xoffs*cpp, cp + (i+yp)*pic_xs+xp,xc*cpp);
		 }
		 break;
		 case 2:
			 for(i = 0; i < yc; i++){
				 memcpy(lfb+(i+yoffs)*stride+xoffs*cpp, sp + (i+yp)*pic_xs+xp, xc*cpp);
			 }
			 break;
		 case 4:
			 for(i = 0; i < yc; i++){
				 memcpy(lfb+(i+yoffs)*stride+xoffs*cpp, ip + (i+yp)*pic_xs+xp, xc*cpp);
			 }
			 break;
	 }
}

void clearFB(int bpp, int cpp)
{
   int x,y;
   getCurrentRes(&x,&y);
	unsigned int stride = CFrameBuffer::getInstance()->getStride();

//printf("clearFB: stride %d y %d cpp %d bpp %d total %d\n", stride, y, cpp, bpp, stride*y*cpp);
	switch(cpp){
		case 2:
			{
				uint32_t  rl, ro, gl, go, bl, bo, tl, to;
				unsigned int i;
				struct fb_var_screeninfo *var;
				var = CFrameBuffer::getInstance()->getScreenInfo();
				rl = (var->red).length;
				ro = (var->red).offset;
				gl = (var->green).length;
				go = (var->green).offset;
				bl = (var->blue).length;
				bo = (var->blue).offset;
				tl = (var->transp).length;
				to = (var->transp).offset;
				short black=make16color(0,0,0, rl, ro, gl, go, bl, bo, tl, to);
				unsigned short *s_fbbuff = (unsigned short *) malloc(y*stride/2 * sizeof(unsigned short));
				if(s_fbbuff==NULL)
				{
					printf("Error: malloc\n");
					return;
				}

				for(i = 0; i < y*stride/2; i++)
				   s_fbbuff[i] = black;
				memcpy(lfb, s_fbbuff, y*stride);
				free(s_fbbuff);
			}
			break;
		case 4:
			{
			uint32_t  col = 0xFF000000;
			uint32_t  * dest = (uint32_t  *) lfb;
			for(unsigned int i = 0; i < stride*y/4; i ++)
				dest[i] = col;
			}
			break;
		default:
			//memset(lfb, 0xFF, stride*y);
			memset(lfb, 0, stride*y);
			break;
	}
}

inline unsigned char make8color(unsigned char r, unsigned char g, unsigned char b)
{
    return (
	(((r >> 5) & 7) << 5) |
	(((g >> 5) & 7) << 2) |
	 ((b >> 6) & 3)       );
}

inline unsigned short make15color(unsigned char r, unsigned char g, unsigned char b)
{
    return ( 
	(((b >> 3) & 31) << 10) |
	(((g >> 3) & 31) << 5)  |
	 ((r >> 3) & 31)        );
}

inline unsigned short make16color(uint32_t  r, uint32_t  g, uint32_t  b, 
											 uint32_t  rl, uint32_t  ro, 
											 uint32_t  gl, uint32_t  go, 
											 uint32_t  bl, uint32_t  bo, 
											 uint32_t  tl, uint32_t  to)
{
    return (
	 // ((0xFF >> (8 - tl)) << to) |
	    ((r    >> (8 - rl)) << ro) |
	    ((g    >> (8 - gl)) << go) |
	    ((b    >> (8 - bl)) << bo));
}

void* convertRGB2FB(unsigned char *rgbbuff, unsigned long x, unsigned long y, int bpp, int *cpp, int transp)
{
    unsigned long i;
    void *fbbuff = NULL;
    unsigned char *c_fbbuff;
    unsigned short *s_fbbuff;
    unsigned int *i_fbbuff;
	 unsigned long count = x*y;
    uint32_t  rl, ro, gl, go, bl, bo, tl, to;
    
	 struct fb_var_screeninfo *var;
    var = CFrameBuffer::getInstance()->getScreenInfo();
    rl = (var->red).length;
    ro = (var->red).offset;
    gl = (var->green).length;
    go = (var->green).offset;
    bl = (var->blue).length;
    bo = (var->blue).offset;
    tl = (var->transp).length;
    to = (var->transp).offset;
	 
	 switch(bpp)
    {
	case 8:
	    *cpp = 1;
	    c_fbbuff = (unsigned char *) malloc(count * sizeof(unsigned char));
		 if(c_fbbuff==NULL)
		 {
			 printf("Error: malloc\n");
			 return NULL;
		 }
	    for(i = 0; i < count; i++)
		c_fbbuff[i] = make8color(rgbbuff[i*3], rgbbuff[i*3+1], rgbbuff[i*3+2]);
	    fbbuff = (void *) c_fbbuff;
	    break;
	case 15:
	    *cpp = 2;
#if HAVE_DVB_API_VERSION < 3
	    s_fbbuff = (unsigned short *) malloc(count * sizeof(unsigned short));
		 if(s_fbbuff==NULL)
		 {
			 printf("Error: malloc\n");
			 return NULL;
		 }
	    for(i = 0; i < count ; i++)
			 s_fbbuff[i] = make15color(rgbbuff[i*3], rgbbuff[i*3+1], rgbbuff[i*3+2]);
#else
		 s_fbbuff = (unsigned short*) make15color_errdiff(rgbbuff, x, y);
#endif
	    fbbuff = (void *) s_fbbuff;
	    break;
	case 16:
	    *cpp = 2;
#if HAVE_DVB_API_VERSION < 3
	    s_fbbuff = (unsigned short *) malloc(count * sizeof(unsigned short));
		 if(s_fbbuff==NULL)
		 {
			 printf("Error: malloc\n");
			 return NULL;
		 }
	    for(i = 0; i < count ; i++)
		 s_fbbuff[i]=make16color(rgbbuff[i*3], rgbbuff[i*3+1], rgbbuff[i*3+2], rl, ro, gl, go, bl, bo, tl, to);
#else
		 s_fbbuff = (unsigned short*) make15color_errdiff(rgbbuff, x, y);
#endif
		 fbbuff = (void *) s_fbbuff;
	    break;
	case 24:
	case 32:
	    *cpp = 4;
	    i_fbbuff = (unsigned int *) malloc(count * sizeof(unsigned int));
		 if(i_fbbuff==NULL)
		 {
			 printf("Error: malloc\n");
			 return NULL;
		 }
	    // red rgbbuff[i*3] green rgbbuff[i*3+1] blue rgbbuff[i*3+2]
	    for(i = 0; i < count ; i++)
		i_fbbuff[i] = (transp << 24) | ((rgbbuff[i*3] << 16) & 0xFF0000) | ((rgbbuff[i*3+1] << 8) & 0xFF00) | (rgbbuff[i*3+2] & 0xFF);
		//i_fbbuff[i] = (transp << 24) | ((rgbbuff[i*3+2] << 16) & 0xFF0000) | ((rgbbuff[i*3+1] << 8) & 0xFF00) | (rgbbuff[i*3] & 0xFF);
	    fbbuff = (void *) i_fbbuff;
	    break;
	default:
	    fprintf(stderr, "Unsupported video mode! You've got: %dbpp\n", bpp);
	    exit(1);
    }
    return fbbuff;
}
