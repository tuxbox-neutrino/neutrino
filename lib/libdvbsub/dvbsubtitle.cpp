/*
 * dvbsubtitle.c: DVB subtitles
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original author: Marco Schlüßler <marco@lordzodiac.de>
 * With some input from the "subtitle plugin" by Pekka Virtanen <pekka.virtanen@sci.fi>
 *
 * $Id: dvbsubtitle.cpp,v 1.1 2009/02/23 19:46:44 rhabarber1848 Exp $
 * dvbsubtitle for HD1 ported by Coolstream LTD
 */

#include "dvbsubtitle.h"

extern "C" {
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
#include <driver/framebuffer.h>
#include "Debug.hpp"

#if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(57, 1, 99)
	#define CODEC_DVB_SUB CODEC_ID_DVB_SUBTITLE
#else
	#define CODEC_DVB_SUB AV_CODEC_ID_DVB_SUBTITLE
#endif

// Set these to 'true' for debug output:
static bool DebugConverter = true;

#define dbgconverter(a...) if (DebugConverter) sub_debug.print(Debug::VERBOSE, a)

class cDvbSubtitleBitmaps : public cListObject 
{
	private:
		int64_t pts;
		AVSubtitle sub;
	public:
		cDvbSubtitleBitmaps(int64_t Pts);
		~cDvbSubtitleBitmaps();
		int64_t Pts(void) { return pts; }
		int Timeout(void) { return sub.end_display_time; }
		void Draw(int &min_x, int &min_y, int &max_x, int &max_y);
		int Count(void) { return sub.num_rects; };
		AVSubtitle * GetSub(void) { return &sub; };
};

cDvbSubtitleBitmaps::cDvbSubtitleBitmaps(int64_t pPts)
{
	//dbgconverter("cDvbSubtitleBitmaps::new: PTS: %lld\n", pts);
	pts = pPts;
}

cDvbSubtitleBitmaps::~cDvbSubtitleBitmaps()
{
    dbgconverter("cDvbSubtitleBitmaps::delete: PTS: %lld rects %d\n", pts, Count());
    int i;

    if(sub.rects) {
	    for (i = 0; i < Count(); i++)
	    {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 5, 0)
		    av_freep(&sub.rects[i]->pict.data[0]);
		    av_freep(&sub.rects[i]->pict.data[1]);
#else
		    av_freep(&sub.rects[i]->data[0]);
		    av_freep(&sub.rects[i]->data[1]);
#endif
		    av_freep(&sub.rects[i]);
	    }

	    av_free(sub.rects);
    }
    memset(&sub, 0, sizeof(AVSubtitle));
}

fb_pixel_t * simple_resize32(uint8_t * orgin, uint32_t * colors, int nb_colors, int ox, int oy, int dx, int dy)
{
	fb_pixel_t  *cr,*l;
	int i,j,k,ip;

	cr = (fb_pixel_t *) malloc(dx*dy*sizeof(fb_pixel_t));

	if(cr == NULL) {
		printf("Error: malloc\n");
		return NULL;
	}
	l = cr;

	for(j = 0; j < dy; j++, l += dx)
	{
		uint8_t * p = orgin + (j*oy/dy*ox);
		for(i = 0, k = 0; i < dx; i++, k++) {
			ip = i*ox/dx;
			int idx = p[ip];
			if(idx < nb_colors)
				l[k] = colors[idx];
		}
	}
	return(cr);
}

void cDvbSubtitleBitmaps::Draw(int &min_x, int &min_y, int &max_x, int &max_y)
{
	int i;
	int stride = CFrameBuffer::getInstance()->getScreenWidth(true);
	uint32_t *sublfb = CFrameBuffer::getInstance()->getFrameBufferPointer();
	int sw = CFrameBuffer::getInstance()->getScreenWidth(true);
	int sh = CFrameBuffer::getInstance()->getScreenHeight(true);

	for (i = 0; i < Count(); i++) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 5, 0)
		uint32_t * colors = (uint32_t *) sub.rects[i]->pict.data[1];
#else
		uint32_t * colors = (uint32_t *) sub.rects[i]->data[1];
#endif
		int width = sub.rects[i]->w;
		int height = sub.rects[i]->h;
		int xoff, yoff;

		int h2 = 576;
		switch (width)
		{
			case 1280:	h2 = 720;  break;
			case 1920:	h2 = 1080; break;
		}

		xoff = sub.rects[i]->x * sw / width;
		yoff = sub.rects[i]->y * sh / h2;
		int nw = width * sw / width;
		int nh = height * sh / h2;

		dbgconverter("cDvbSubtitleBitmaps::Draw: #%d at %d,%d size %dx%d colors %d (x=%d y=%d w=%d h=%d) \n", i+1, 
				sub.rects[i]->x, sub.rects[i]->y, sub.rects[i]->w, sub.rects[i]->h, sub.rects[i]->nb_colors, xoff, yoff, nw, nh);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 5, 0)
		fb_pixel_t * newdata = simple_resize32 (sub.rects[i]->pict.data[0], colors, sub.rects[i]->nb_colors, width, height, nw, nh);
#else
		fb_pixel_t * newdata = simple_resize32 (sub.rects[i]->data[0], colors, sub.rects[i]->nb_colors, width, height, nw, nh);
#endif

		fb_pixel_t * ptr = newdata;
		for (int y2 = 0; y2 < nh; y2++) {
			int y = (yoff + y2) * stride;
			for (int x2 = 0; x2 < nw; x2++)
				*(sublfb + xoff + x2 + y) = *ptr++;
		}
		free(newdata);

		if(min_x > xoff)
			min_x = xoff;
		if(min_y > yoff)
			min_y = yoff;
		if(max_x < (xoff + nw))
			max_x = xoff + nw;
		if(max_y < (yoff + nh))
			max_y = yoff + nh;
	}

	if(Count())
		dbgconverter("cDvbSubtitleBitmaps::Draw: finish, min/max screen: x=% d y= %d, w= %d, h= %d\n", min_x, min_y, max_x-min_x, max_y-min_y);
	dbgconverter("\n");
}

static int screen_w, screen_h, screen_x, screen_y;

cDvbSubtitleConverter::cDvbSubtitleConverter(void)
{
	dbgconverter("cDvbSubtitleConverter: new converter\n");

	bitmaps = new cList<cDvbSubtitleBitmaps>;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&mutex, &attr);
	running = false;

	avctx = NULL;
	avcodec = NULL;

	avcodec_register_all();
	avcodec = avcodec_find_decoder(CODEC_DVB_SUB);//CODEC_ID_DVB_SUBTITLE or AV_CODEC_ID_DVB_SUBTITLE from 57.1.100
	if (!avcodec) {
		dbgconverter("cDvbSubtitleConverter: unable to get dvb subtitle codec!\n");
		return;
	}
	avctx = avcodec_alloc_context3(avcodec);
	if (avcodec_open2(avctx, avcodec, NULL) < 0)
		dbgconverter("cDvbSubtitleConverter: unable to open codec !\n");

	av_log_set_level(AV_LOG_PANIC);
	//if(DebugConverter)
	//	av_log_set_level(AV_LOG_INFO);

	screen_w = min_x = CFrameBuffer::getInstance()->getScreenWidth();
	screen_h = min_y = CFrameBuffer::getInstance()->getScreenHeight();
	screen_x = max_x = CFrameBuffer::getInstance()->getScreenX();
	screen_y = max_y = CFrameBuffer::getInstance()->getScreenY();
	Timeout.Set(0xFFFF*1000);
}

cDvbSubtitleConverter::~cDvbSubtitleConverter()
{
	if (avctx) {
		avcodec_close(avctx);
		av_free(avctx);
		avctx = NULL;
	}
	delete bitmaps;
}

void cDvbSubtitleConverter::Lock(void)
{
  pthread_mutex_lock(&mutex);
}

void cDvbSubtitleConverter::Unlock(void)
{
  pthread_mutex_unlock(&mutex);
}

void cDvbSubtitleConverter::Pause(bool pause)
{
	dbgconverter("cDvbSubtitleConverter::Pause: %s\n", pause ? "pause" : "resume");
	if(pause) {
		if(!running)
			return;
		Lock();
		Clear();
		running = false;
		Unlock();
	} else {
		running = true;
	}
}

void cDvbSubtitleConverter::Clear(void)
{
	if(running && (max_x-min_x > 0) && (max_y-min_y > 0)) {
		dbgconverter("cDvbSubtitleConverter::Clear: x=% d y= %d, w= %d, h= %d\n", min_x, min_y, max_x-min_x, max_y-min_y);
		CFrameBuffer::getInstance()->paintBackgroundBoxRel (min_x, min_y, max_x-min_x, max_y-min_y);
		/* reset area to clear */
		min_x = screen_w;
		min_y = screen_h;
		max_x = screen_x;
		max_y = screen_y;
	}
}

void cDvbSubtitleConverter::Reset(void)
{
	dbgconverter("Converter reset -----------------------\n");
	Lock();
	bitmaps->Clear();
	Unlock();
	Timeout.Set(0xFFFF*1000);
}

int cDvbSubtitleConverter::Convert(const uchar *Data, int Length, int64_t pts)
{
	AVPacket avpkt;
	int got_subtitle = 0;
	static cDvbSubtitleBitmaps *Bitmaps = NULL;

	if(!avctx) {
		dbgconverter("cDvbSubtitleConverter::Convert: no context\n");
		return -1;
	}

	if(Bitmaps == NULL)
		Bitmaps = new cDvbSubtitleBitmaps(pts);

 	AVSubtitle * sub = Bitmaps->GetSub();

	av_init_packet(&avpkt);
	avpkt.data = (uint8_t*) Data;
	avpkt.size = Length;

	dbgconverter("cDvbSubtitleConverter::Convert: sub %x pkt %x pts %lld\n", sub, &avpkt, pts);

	avcodec_decode_subtitle2(avctx, sub, &got_subtitle, &avpkt);
	dbgconverter("cDvbSubtitleConverter::Convert: pts %lld subs ? %s, %d bitmaps\n", pts, got_subtitle? "yes" : "no", sub->num_rects);

	if(got_subtitle) {
		if(DebugConverter) {
			unsigned int i;
			for(i = 0; i < sub->num_rects; i++) {
				dbgconverter("cDvbSubtitleConverter::Convert: #%d at %d,%d size %d x %d colors %d\n", i+1, 
						sub->rects[i]->x, sub->rects[i]->y, sub->rects[i]->w, sub->rects[i]->h, sub->rects[i]->nb_colors);
			}
		}
		bitmaps->Add(Bitmaps);
		Bitmaps = NULL;
	}

	return 0;
}

#define LimitTo32Bit(n) (n & 0x00000000FFFFFFFFL)
#define MAXDELTA 40000 // max. reasonable PTS/STC delta in ms
#define MIN_DISPLAY_TIME 1500
#define SHOW_DELTA 20
#define WAITMS 500

void dvbsub_get_stc(int64_t * STC);

int cDvbSubtitleConverter::Action(void)
{
	int WaitMs = WAITMS;

	if (!running)
		return 0;

	if(!avctx) {
		dbgconverter("cDvbSubtitleConverter::Action: no context\n");
		return -1;
	}

	Lock();
	if (cDvbSubtitleBitmaps *sb = bitmaps->First()) {
		int64_t STC;
		dvbsub_get_stc(&STC);
		int64_t Delta = 0;

		Delta = LimitTo32Bit(sb->Pts()) - LimitTo32Bit(STC);
		Delta /= 90; // STC and PTS are in 1/90000s
		dbgconverter("cDvbSubtitleConverter::Action: PTS: %016llx STC: %016llx (%lld) timeout: %d\n", sb->Pts(), STC, Delta, sb->Timeout());

		if (Delta <= MAXDELTA) {
			if (Delta <= SHOW_DELTA) {
				dbgconverter("cDvbSubtitleConverter::Action: PTS: %012llx STC: %012llx (%lld) timeout: %d bmp %d/%d\n", sb->Pts(), STC, Delta, sb->Timeout(), bitmaps->Count(), sb->Index() + 1);
				dbgconverter("cDvbSubtitleConverter::Action: Got %d bitmaps, showing #%d\n", bitmaps->Count(), sb->Index() + 1);
				if (running) {
					Clear();
					sb->Draw(min_x, min_y, max_x, max_y);
					Timeout.Set(sb->Timeout());
				}
				if(sb->Count())
					WaitMs = MIN_DISPLAY_TIME;
				bitmaps->Del(sb, true);
			}
			else if (Delta < WaitMs)
				WaitMs = int((Delta > SHOW_DELTA) ? Delta - SHOW_DELTA : Delta);
		}
		else
		{
			dbgconverter("deleted because delta (%lld) > MAXDELTA (%d)\n", Delta, MAXDELTA);
			bitmaps->Del(sb, true);
		}
	} else {
		if (Timeout.TimedOut()) {
			dbgconverter("cDvbSubtitleConverter::Action: timeout, elapsed %lld\n", Timeout.Elapsed());
			Clear();
			Timeout.Set(0xFFFF*1000);
		}
	}
	Unlock();
	if(WaitMs != WAITMS)
		dbgconverter("cDvbSubtitleConverter::Action: finish, WaitMs %d\n", WaitMs);

	return WaitMs*1000;
}
