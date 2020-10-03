#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <math.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#if HAVE_CST_HARDWARE
#include <driver/vfd.h>
#endif

#include "int_fft.c"

typedef signed short gint16;
typedef int gint;
typedef float gfloat;
typedef double gdouble;

#define NUM_BANDS 16
#define WIDTH (120/NUM_BANDS)
#define HEIGHT 64
#define FALL 2
#define FALLOFF 1

#if 0
static int bar_heights[NUM_BANDS];
static int falloffs[NUM_BANDS];
static gdouble scale = 0;

static gint xscale128[] = { 0, 1, 2, 3, 4, 6, 8, 9, 10, 14, 20, 27, 37, 50, 67, 94, 127 };
static gint xscale256[] = { 0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255 };
static gint xscale512[] = { 0, 2, 4, 6, 10, 14, 20, 28, 40, 56, 80, 108, 148, 202, 274, 374, 510 };
#endif
//#define DEBUG
#if HAVE_DBOX2
#define SAMPLES 256
#define LOG 8
#define xscale xscale128
#else
#define SAMPLES 512
#define LOG 9
#define xscale xscale256
#endif

#if 0
static void do_fft(gint16 out_data[], gint16 in_data[])
{
	gint16 im[SAMPLES];

	memset(im, 0, sizeof(im));
	window(in_data, SAMPLES);
	fix_fft(in_data, im, LOG, 0);
	fix_loud(out_data, in_data, im, SAMPLES/2, 0);
}
#endif
//static int threshold = -60;
void sanalyzer_render_freq (gint16 /*in_data*/[])
{
#if 0
  gint i, c;
  gint y;
  gint16 freq_data[1024];
  static int fl = 0;

#ifdef DEBUG
struct timeval tv1, tv2;
gettimeofday (&tv1, NULL);
#endif
  do_fft (freq_data, in_data);

  CVFD::getInstance ()->Clear ();
#ifdef DEBUG
//gettimeofday (&tv1, NULL);
#endif
  for (i = 0; i < NUM_BANDS; i++) {
	y = 0;

#if 0 // using max value
        for (c = xscale[i]; c < xscale[i + 1]; c++) {
          if(freq_data[c] > threshold) {
            int val = freq_data[c]-threshold;
            if (val > y)
                y = val;
          }
        }
#else
        int val = 0;
        int cnt = 0;
        for (c = xscale[i]; c < xscale[i + 1]; c++) {
          if(freq_data[c] > threshold) {
            val += freq_data[c]-threshold;
            cnt++;
          }
        }
        if(cnt) y = val/cnt;
#endif

	if (y > HEIGHT - 1)
		y = HEIGHT - 1;

	if (y > bar_heights[i])
	  bar_heights[i] = y;
	else if (bar_heights[i] > FALL)
	  bar_heights[i] -= FALL;
	else
	  bar_heights[i] = 0;

	if (y > falloffs[i])
	  falloffs[i] = y;
	else if (falloffs[i] > FALLOFF) {
		fl ++;
		if(fl > 2) {
			fl = 0;
			falloffs[i] -= FALLOFF;
		}
	}
	else
	  falloffs[i] = 0;
  	CVFD::getInstance ()->drawBar (i*WIDTH, 64-falloffs[i]-5, WIDTH, 2);
	y = bar_heights[i];
  	CVFD::getInstance ()->drawBar (i*WIDTH, 64-y, WIDTH, y);
  }

#ifdef DEBUG
gettimeofday (&tv2, NULL);
printf("Calc takes %dns\n", (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec));
#endif
  CVFD::getInstance ()->Update ();
#endif
}
#if 0
void sanalyzer_render_vu (gint16 in_data[2][512])
{
  long delay_value;
  int a = 0,b = 0, c;
  float max = 32767.50;
  float db_min = (float) -91;
  float channel_data[2] = { 0.0, 0.0};
  static int olda = 0, oldb = 0;
return;
#ifdef DEBUG
//struct timeval tv1, tv2;
//gettimeofday (&tv1, NULL);
#endif
  for (c = 0; c < 512; c += 2) {
	channel_data[0] += (float) (in_data[0][c] * in_data[0][c]);
	channel_data[1] += (float) (in_data[1][c] * in_data[1][c]);
  }
  channel_data[0] = 20 * log10 (sqrt (channel_data[0] / 256.0) / max);
  channel_data[1] = 20 * log10 (sqrt (channel_data[1] / 256.0) / max);

  a = channel_data[0] + 91;
  b = channel_data[1] + 91;
//printf ("channel0: %d\n", a);
//printf ("channel1: %d\n", b);
#ifdef DEBUG
//gettimeofday (&tv2, NULL);
//printf("Calc takes %dms\n", (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_usec - tv1.tv_usec) / 1000);
#endif
  if(a != olda || b != oldb) {
  	CVFD::getInstance ()->Clear ();
  	CVFD::getInstance ()->drawBar (0, 8, a, 20);
  	CVFD::getInstance ()->drawBar (0, 36, b, 20);
#ifdef DEBUG
//gettimeofday (&tv1, NULL);
#endif
  	CVFD::getInstance ()->Update ();
#ifdef DEBUG
//gettimeofday (&tv2, NULL);
//int ms = (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_usec - tv1.tv_usec) / 1000;
//if(ms) printf("Update takes %dms\n", ms);
#endif
  }
  return;
}
#endif
