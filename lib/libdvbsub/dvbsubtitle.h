/*
 * dvbsubtitle.h: DVB subtitles
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original author: Marco Schlüßler <marco@lordzodiac.de>
 *
 * $Id: dvbsubtitle.h,v 1.1 2009/02/23 19:46:44 rhabarber1848 Exp $
 */

#ifndef __DVBSUBTITLE_H
#define __DVBSUBTITLE_H

extern "C" {
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#include "tools.h"

class cDvbSubtitleBitmaps;

class cDvbSubtitleConverter  /*: public cThread */{
private:
  bool running;
  pthread_mutex_t mutex;
  cList<cDvbSubtitleBitmaps> *bitmaps;
  AVCodecContext * avctx;
  AVCodec * avcodec;
  int min_x, min_y, max_x, max_y;
  cTimeMs Timeout;
public:
  cDvbSubtitleConverter(void);
  virtual ~cDvbSubtitleConverter();
  int Action(void);
  void Reset(void);
  void Clear(void);
  void Pause(bool pause);
  void Lock();
  void Unlock();
  int Convert(const uchar *Data, int Length, int64_t pts);
  bool Running() { return running; };
};


#endif //__DVBSUBTITLE_H
