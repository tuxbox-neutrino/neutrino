#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <global.h>
#include <neutrino.h>
#include <gui/customcolor.h>
#include <gui/scale.h>

#include <string>

#include <time.h>
#include <sys/timeb.h>
#include <sys/param.h>

#define RED_BAR 40
#define YELLOW_BAR 70
#define GREEN_BAR 100

#define BAR_BORDER 1
#define BARW 2
#define BARWW 2

#define ITEMW 4
#define POINT 2

#define RED    0xFF0000
#define GREEN  0x00FF00
#define YELLOW 0xFFFF00

inline unsigned int make16color(__u32 rgb)
{
        return 0xFF000000 | rgb;
}

CScale::CScale (int w, int h, int r, int g, int b, bool inv)
{
  width = w;
  height = h;
  inverse = inv;
#ifndef NO_BLINKENLIGHTS
  double div;
//printf("new SCALE, w %d h %d size %d\n", w, h, sizeof(CScale)); fflush(stdout);
  frameBuffer = CFrameBuffer::getInstance ();
  div = (double) 100 / (double) width;
  red = (double) r / (double) div / (double) ITEMW;
  green = (double) g / (double) div / (double) ITEMW;
  yellow = (double) b / (double) div / (double) ITEMW;
#endif
  percent = 255;
}

#ifndef NO_BLINKENLIGHTS
void CScale::paint (int x, int y, int pcr)
{
  int i, j, siglen;
  int posx, posy;
  int xpos, ypos;
  int hcnt = height / ITEMW;
  double div;
  uint32_t  rgb;
  
  fb_pixel_t color;
  int b = 0;

  i = 0;
  xpos = x;
  ypos = y;
//printf("CScale::paint: old %d new %d x %d y %d\n", percent, pcr, x, y); fflush(stdout);
  if (pcr != percent) {
	if(percent == 255) percent = 0;
	div = (double) 100 / (double) width;
	siglen = (double) pcr / (double) div;
	posx = xpos;
	posy = ypos;
	int maxi = siglen / ITEMW;
	int total = width / ITEMW;
	int step = 255/total;
	if (pcr > percent) {

		for (i = 0; (i < red) && (i < maxi); i++) {
		  step = 255/red;
		  if(inverse) rgb = GREEN + ((unsigned char)(step*i) << 16); // adding red
		  else rgb = RED + ((unsigned char)(step*i) << 8); // adding green
		  color = make16color(rgb);
		  for(j = 0; j <= hcnt; j++ ) {
			frameBuffer->paintBoxRel (posx + i*ITEMW, posy + j*ITEMW, POINT, POINT, color);
		  }
		}
//printf("hcnt %d yellow %d i %d\n", hcnt, yellow, i); fflush(stdout);
		for (; (i < yellow) && (i < maxi); i++) {
		  step = 255/yellow/2;
		  if(inverse) rgb = YELLOW - (((unsigned char)step*(b++)) << 8); // removing green
		  else rgb = YELLOW - ((unsigned char)(step*(b++)) << 16); // removing red
		  color = make16color(rgb);
//printf("YELLOW: or %08X diff %08X result %08X\n", YELLOW, ((unsigned char)(step*(b-1)) << 16), color);
		  for(j = 0; j <= hcnt; j++ ) {
			frameBuffer->paintBoxRel (posx + i*ITEMW, posy + j*ITEMW, POINT, POINT, color);
		  }
		}
		for (; (i < green) && (i < maxi); i++) {
		  step = 255/green;
		  if(inverse) rgb = YELLOW - ((unsigned char) (step*(b++)) << 8); // removing green
		  else rgb = YELLOW - ((unsigned char) (step*(b++)) << 16); // removing red
		  color = make16color(rgb);
		  for(j = 0; j <= hcnt; j++ ) {
			frameBuffer->paintBoxRel (posx + i*ITEMW, posy + j*ITEMW, POINT, POINT, color);
		  }
		}
	}
	for(i = maxi; i < total; i++) {
	  for(j = 0; j <= hcnt; j++ ) {
		frameBuffer->paintBoxRel (posx + i*ITEMW, posy + j*ITEMW, POINT, POINT, COL_INFOBAR_PLUS_3);//fill passive
	  }
	}
	percent = pcr;
  }
}
#else
void CScale::paint (int x, int y, int pcr)
{
	if (pcr == percent)
		return;
	percent = pcr;
	pb.paintProgressBar(x, y, width, height, pcr * width / 100, width, COL_INFOBAR_PLUS_3, COL_INFOBAR_PLUS_0, COL_INFOBAR_PLUS_3);
}
#endif

void CScale::reset ()
{
  percent = 255;
}

void CScale::hide ()
{
}
