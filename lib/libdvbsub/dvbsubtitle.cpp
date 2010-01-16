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
#include "driver/framebuffer.h"
#include "Debug.hpp"

// Set these to 'true' for debug output:
static bool DebugConverter = true;

#define dbgconverter(a...) if (DebugConverter) sub_debug.print(Debug::VERBOSE, a)

// --- cDvbSubtitleBitmaps ---------------------------------------------------

class cDvbSubtitleBitmaps : public cListObject 
{
	private:
		int64_t pts;
		int timeout;
		AVSubtitle sub;
		//  int min_x, min_y, max_x, max_y;
	public:
		cDvbSubtitleBitmaps(int64_t Pts);
		~cDvbSubtitleBitmaps();
		int64_t Pts(void) { return pts; }
		int Timeout(void) { return sub.end_display_time; }
		void Draw();
		void Clear(void);
		int Count(void) { return sub.num_rects; };
		AVSubtitle * GetSub(void) { return &sub; };
};

cDvbSubtitleBitmaps::cDvbSubtitleBitmaps(int64_t pPts)
{
	pts = pPts;
	//  max_x = max_y = 0;
	//  min_x = min_y = 0xFFFF;
	//dbgconverter("cDvbSubtitleBitmaps::new: PTS: %lld\n", pts);
}

cDvbSubtitleBitmaps::~cDvbSubtitleBitmaps()
{
    dbgconverter("cDvbSubtitleBitmaps::delete: PTS: %lld rects %d\n", pts, Count());
    int i;

    //return; //FIXME !

    for (i = 0; i < Count(); i++)
    {
        av_freep(&sub.rects[i]->pict.data[0]);
        av_freep(&sub.rects[i]->pict.data[1]);
        av_freep(&sub.rects[i]);
    }

    av_free(sub.rects);

    memset(&sub, 0, sizeof(AVSubtitle));
}

static int min_x = 0xFFFF, min_y = 0xFFFF;
static int max_x = 0, max_y = 0;

void cDvbSubtitleBitmaps::Clear()
{
	dbgconverter("cDvbSubtitleBitmaps::Clear: x=% d y= %d, w= %d, h= %d\n", min_x, min_y, max_x-min_x, max_y-min_y);
	if(max_x && max_y) {
		CFrameBuffer::getInstance()->paintBackgroundBoxRel (min_x, min_y-10, max_x-min_x, max_y-min_y+10);
		//CFrameBuffer::getInstance()->paintBackground();
		//max_x = max_y = 0;
		//min_x = min_y = 0xFFFF;
	}
}

void cDvbSubtitleBitmaps::Draw()
{
	int i;
	int stride = CFrameBuffer::getInstance()->getScreenWidth(true);
	int wd = CFrameBuffer::getInstance()->getScreenWidth();
	int xs = CFrameBuffer::getInstance()->getScreenX();
	int yend = CFrameBuffer::getInstance()->getScreenY() + CFrameBuffer::getInstance()->getScreenHeight();
	uint32_t *sublfb = CFrameBuffer::getInstance()->getFrameBufferPointer();

	dbgconverter("cDvbSubtitleBitmaps::Draw: %d bitmaps, x= %d, width= %d end=%d stride %d\n", sub.num_rects, xs, wd, yend, stride);

	//if(Count())
		Clear(); //FIXME should we clear for new bitmaps set ?

	for (i = 0; i < Count(); i++) {
		/* center on screen */
		int xoff = xs + (wd - sub.rects[i]->w) / 2;
		/* move to screen bottom */
		int yoff = (yend - (576 - sub.rects[i]->y)) * stride;
		int ys = yend - (576 - sub.rects[i]->y);

		dbgconverter("cDvbSubtitleBitmaps::Draw: #%d at %d,%d size %dx%d colors %d (x=%d y=%d) \n", i+1, 
				sub.rects[i]->x, sub.rects[i]->y, sub.rects[i]->w, sub.rects[i]->h, sub.rects[i]->nb_colors, xoff, ys);

		uint32_t * colors = (uint32_t *) sub.rects[i]->pict.data[1];
		for (int y2 = 0; y2 < sub.rects[i]->h; y2++) {
			int y = y2*stride + yoff;
			for (int x2 = 0; x2 < sub.rects[i]->w; x2++)
			{
				int idx = sub.rects[i]->pict.data[0][y2*sub.rects[i]->w+x2];// *(bitmaps[i]->Data(x2, y2));
				if(idx > sub.rects[i]->nb_colors) {
					dbgconverter("cDvbSubtitleBitmaps::Draw pixel at %d x %d is %d!\n", x2, y2, idx);
				} else {
					*(sublfb + xoff + x2 + y) = colors[idx];
				}
			}
		}
		if(min_x > xoff)
			min_x = xoff;
		if(min_y > ys)
			min_y = ys;
		if(max_x < (xoff + sub.rects[i]->w))
			max_x = xoff + sub.rects[i]->w;
		if(max_y < (ys + sub.rects[i]->h))
			max_y = ys + sub.rects[i]->h;
	}
	if(Count())
		dbgconverter("cDvbSubtitleBitmaps::Draw: finish, min/max screen: x=% d y= %d, w= %d, h= %d\n", min_x, min_y, max_x-min_x, max_y-min_y);
	dbgconverter("\n");
}

// --- cDvbSubtitleConverter -------------------------------------------------

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
	avcodec = avcodec_find_decoder(CODEC_ID_DVB_SUBTITLE);
	if (!avcodec) {
		dbgconverter("cDvbSubtitleConverter: unable to get dvb subtitle codec!\n");
		return;
	}
	avctx = avcodec_alloc_context();
	if (avcodec_open(avctx, avcodec) < 0)
		dbgconverter("cDvbSubtitleConverter: unable to open codec !\n");

	av_log_set_level(99);
	if(DebugConverter)
		av_log_set_level(AV_LOG_INFO);
}

cDvbSubtitleConverter::~cDvbSubtitleConverter()
{
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
		running = false;
		Clear();
		Unlock();
		Reset();
	} else {
		running = true;
	}
}

void cDvbSubtitleConverter::Clear(void)
{
	if(max_x && max_y) {
		dbgconverter("cDvbSubtitleConverter::Clear: x=% d y= %d, w= %d, h= %d\n", min_x, min_y, max_x-min_x, max_y-min_y);
		CFrameBuffer::getInstance()->paintBackgroundBoxRel (min_x, min_y-10, max_x-min_x, max_y-min_y+10);
		//CFrameBuffer::getInstance()->paintBackground();
		//max_x = max_y = 0;
		//min_x = min_y = 0xFFFF;
	}
}

void cDvbSubtitleConverter::Reset(void)
{
  dbgconverter("Converter reset -----------------------\n");
  Lock();
  bitmaps->Clear();
  Unlock();
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

	dbgconverter("cDvbSubtitleConverter::Convert: sub %x pkt %x\n", sub, &avpkt);
	//avctx->sub_id = (anc_page << 16) | comp_page; //FIXME not patched ffmpeg needs this !

	avcodec_decode_subtitle2(avctx, sub, &got_subtitle, &avpkt);
	dbgconverter("cDvbSubtitleConverter::Convert: subs ? %s, %d bitmaps\n", got_subtitle? "yes" : "no", sub->num_rects);

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
	static cTimeMs Timeout(0xFFFF*1000);
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
		dbgconverter("cDvbSubtitleConverter::Action: PTS: %lld  STC: %lld (%lld) timeout: %d\n", sb->Pts(), STC, Delta, sb->Timeout());

		if (Delta <= MAXDELTA) {
			if (Delta <= SHOW_DELTA) {
				dbgconverter("cDvbSubtitleConverter::Action: Got %d bitmaps, showing #%d\n", bitmaps->Count(), sb->Index() + 1);
				if (running) {
					sb->Draw();
					Timeout.Set(sb->Timeout());
				}
				if(sb->Count())
					WaitMs = MIN_DISPLAY_TIME;
				bitmaps->Del(sb, true);
			}
			else if (Delta < WaitMs)
				WaitMs = (Delta > SHOW_DELTA) ? Delta - SHOW_DELTA : Delta;
		}
		else
			bitmaps->Del(sb, true);
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
