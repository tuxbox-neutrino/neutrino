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

#include "osd.h"
#include "tools.h"

class cDvbSubtitlePage;
class cDvbSubtitleAssembler; // for legacy PES recordings
class cDvbSubtitleBitmaps;

class cDvbSubtitleConverter  /*: public cThread */{
private:
  static int setupLevel;
  cDvbSubtitleAssembler *dvbSubtitleAssembler;
//  cOsd *osd;
  cList<cDvbSubtitlePage> *pages;
  cList<cDvbSubtitleBitmaps> *bitmaps;
  tColor yuv2rgb(int Y, int Cb, int Cr);
  bool AssertOsd(void);
  int ExtractSegment(const uchar *Data, int Length, int64_t Pts);
  void FinishPage(cDvbSubtitlePage *Page);
  bool running;
  pthread_mutex_t mutex;
public:
  cDvbSubtitleConverter(void);
  virtual ~cDvbSubtitleConverter();
  void Action(void);
  void Reset(void);
  void Clear(void);
  void Pause(bool pause);
  void Lock();
  void Unlock();
  int ConvertFragments(const uchar *Data, int Length, int64_t pts); // for legacy PES recordings
  int Convert(const uchar *Data, int Length, int64_t pts);
  static void SetupChanged(void);
  bool Running() { return running; };
};


#endif //__DVBSUBTITLE_H
