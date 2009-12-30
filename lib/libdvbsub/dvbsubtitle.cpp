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
}
#include "driver/framebuffer.h"
#include "Debug.hpp"

#define FB	"/dev/fb/0"
extern int fb_fd;
//struct fb_fix_screeninfo fix_screeninfo;
//struct fb_var_screeninfo var_screeninfo;

#define PAGE_COMPOSITION_SEGMENT   0x10
#define REGION_COMPOSITION_SEGMENT 0x11
#define CLUT_DEFINITION_SEGMENT    0x12
#define OBJECT_DATA_SEGMENT        0x13
#define END_OF_DISPLAY_SET_SEGMENT 0x80

// Set these to 'true' for debug output:
#if 1
static bool DebugConverter = true;
static bool DebugSegments = false;
static bool DebugPages = false;
static bool DebugRegions = false;
static bool DebugObjects = false;
static bool DebugCluts = false;
#else
static bool DebugConverter = true;
static bool DebugSegments = true;
static bool DebugPages = true;
static bool DebugRegions = true;
static bool DebugObjects = true;
static bool DebugCluts = true;
#endif

#if 0
#define dbgconverter(a...) if (DebugConverter) fprintf(stderr, a)
#define dbgsegments(a...) if (DebugSegments) fprintf(stderr, a)
#define dbgpages(a...) if (DebugPages) fprintf(stderr, a)
#define dbgregions(a...) if (DebugRegions) fprintf(stderr, a)
#define dbgobjects(a...) if (DebugObjects) fprintf(stderr, a)
#define dbgcluts(a...) if (DebugCluts) fprintf(stderr, a)
#endif

#define dbgconverter(a...) if (DebugConverter) sub_debug.print(Debug::VERBOSE, a)
#define dbgsegments(a...) if (DebugSegments) sub_debug.print(Debug::VERBOSE, a)
#define dbgpages(a...) if (DebugPages) sub_debug.print(Debug::VERBOSE, a)
#define dbgregions(a...) if (DebugRegions) sub_debug.print(Debug::VERBOSE, a)
#define dbgobjects(a...) if (DebugObjects) sub_debug.print(Debug::VERBOSE, a)
#define dbgcluts(a...) if (DebugCluts) sub_debug.print(Debug::VERBOSE, a)

int SubtitleFgTransparency = 0;
int SubtitleBgTransparency = 0;

// --- cSubtitleClut ---------------------------------------------------------

class cSubtitleClut : public cListObject {
private:
  int clutId;
  int version;
  cPalette palette2;
  cPalette palette4;
  cPalette palette8;
public:
  cSubtitleClut(int ClutId);
  int ClutId(void) { return clutId; }
  int Version(void) { return version; }
  void SetVersion(int pVersion) { version = pVersion; }
  void SetColor(int Bpp, int Index, tColor Color);
  const cPalette *GetPalette(int Bpp);
  };

cSubtitleClut::cSubtitleClut(int pClutId)
:palette2(2)
,palette4(4)
,palette8(8)
{
  clutId = pClutId;
  version = -1;
}

void cSubtitleClut::SetColor(int Bpp, int pIndex, tColor Color)
{
  switch (Bpp) {
    case 2: palette2.SetColor(pIndex, Color); break;
    case 4: palette4.SetColor(pIndex, Color); break;
    case 8: palette8.SetColor(pIndex, Color); break;
    default: esyslog("ERROR: wrong Bpp in cSubtitleClut::SetColor(%d, %d, %08X)", Bpp, pIndex, Color);
    }
}

const cPalette *cSubtitleClut::GetPalette(int Bpp)
{
  switch (Bpp) {
    case 2: return &palette2;
    case 4: return &palette4;
    case 8: return &palette8;
    default: esyslog("ERROR: wrong Bpp in cSubtitleClut::GetPalette(%d)", Bpp);
    }
  return &palette8;
}

// --- cSubtitleObject -------------------------------------------------------

class cSubtitleObject : public cListObject {
private:
  int objectId;
  int version;
  int codingMethod;
  bool nonModifyingColorFlag;
  int nibblePos;
  uchar backgroundColor;
  uchar foregroundColor;
  int providerFlag;
  int px;
  int py;
  cBitmap *bitmap;
  void DrawLine(int x, int y, tIndex Index, int Length);
  uchar Get2Bits(const uchar *Data, int &Index);
  uchar Get4Bits(const uchar *Data, int &Index);
  bool Decode2BppCodeString(const uchar *Data, int &Index, int&x, int y);
  bool Decode4BppCodeString(const uchar *Data, int &Index, int&x, int y);
  bool Decode8BppCodeString(const uchar *Data, int &Index, int&x, int y);
public:
  cSubtitleObject(int ObjectId, cBitmap *Bitmap);
  int ObjectId(void) { return objectId; }
  int Version(void) { return version; }
  int CodingMethod(void) { return codingMethod; }
  bool NonModifyingColorFlag(void) { return nonModifyingColorFlag; }
  void DecodeSubBlock(const uchar *Data, int Length, bool Even);
  void SetVersion(int pVersion) { version = pVersion; }
  void SetBackgroundColor(uchar BackgroundColor) { backgroundColor = BackgroundColor; }
  void SetForegroundColor(uchar ForegroundColor) { foregroundColor = ForegroundColor; }
  void SetNonModifyingColorFlag(bool pNonModifyingColorFlag) { nonModifyingColorFlag = pNonModifyingColorFlag; }
  void SetCodingMethod(int pCodingMethod) { codingMethod = pCodingMethod; }
  void SetPosition(int x, int y) { px = x; py = y; }
  void SetProviderFlag(int pProviderFlag) { providerFlag = pProviderFlag; }
  };

cSubtitleObject::cSubtitleObject(int pObjectId, cBitmap *pBitmap)
{
  objectId = pObjectId;
  version = -1;
  codingMethod = -1;
  nonModifyingColorFlag = false;
  nibblePos = 0;
  backgroundColor = 0;
  foregroundColor = 0;
  providerFlag = -1;
  px = py = 0;
  bitmap = pBitmap;
}

void cSubtitleObject::DecodeSubBlock(const uchar *Data, int Length, bool Even)
{
  int x = 0;
  int y = Even ? 0 : 1;
  for (int index = 0; index < Length; ) {
      switch (Data[index++]) {
        case 0x10: {
             nibblePos = 8;
             while (Decode2BppCodeString(Data, index, x, y) && index < Length)
                   ;
             if (!nibblePos)
                index++;
             break;
             }
        case 0x11: {
             nibblePos = 4;
             while (Decode4BppCodeString(Data, index, x, y) && index < Length)
                   ;
             if (!nibblePos)
                index++;
             break;
             }
        case 0x12:
             while (Decode8BppCodeString(Data, index, x, y) && index < Length)
                   ;
             break;
        case 0x20: //TODO
             dbgobjects("sub block 2 to 4 map");
             index += 4;
             break;
        case 0x21: //TODO
             dbgobjects("sub block 2 to 8 map");
             index += 4;
             break;
        case 0x22: //TODO
             dbgobjects("sub block 4 to 8 map");
             index += 16;
             break;
        case 0xF0:
             x = 0;
             y += 2;
             break;
        }
      }
}

void cSubtitleObject::DrawLine(int x, int y, tIndex pIndex, int Length)
{
  if (nonModifyingColorFlag && pIndex == 1)
     return;
  x += px;
  y += py;
  for (int pos = x; pos < x + Length; pos++)
      bitmap->SetIndex(pos, y, pIndex);
}

uchar cSubtitleObject::Get2Bits(const uchar *Data, int &pIndex)
{
  uchar result = Data[pIndex];
  if (!nibblePos) {
     pIndex++;
     nibblePos = 8;
     }
  nibblePos -= 2;
  return (result >> nibblePos) & 0x03;
}

uchar cSubtitleObject::Get4Bits(const uchar *Data, int &pIndex)
{
  uchar result = Data[pIndex];
  if (!nibblePos) {
     pIndex++;
     nibblePos = 4;
     }
  else {
     result >>= 4;
     nibblePos -= 4;
     }
  return result & 0x0F;
}

bool cSubtitleObject::Decode2BppCodeString(const uchar *Data, int &pIndex, int &x, int y)
{
  int rl = 0;
  int color = 0;
  uchar code = Get2Bits(Data, pIndex);
  if (code) {
     color = code;
     rl = 1;
     }
  else {
     code = Get2Bits(Data, pIndex);
     if (code & 2) { // switch_1
        rl = ((code & 1) << 2) + Get2Bits(Data, pIndex) + 3;
        color = Get2Bits(Data, pIndex);
        }
     else if (code & 1)
        rl = 1; //color 0
     else {
        code = Get2Bits(Data, pIndex);
        switch (code & 0x3) { //switch_3
          case 0:
               return false;
          case 1:
               rl = 2; //color 0
               break;
          case 2:
               rl = (Get2Bits(Data, pIndex) << 2) + Get2Bits(Data, pIndex) + 12;
               color = Get2Bits(Data, pIndex);
               break;
          case 3:
               rl = (Get2Bits(Data, pIndex) << 6) + (Get2Bits(Data, pIndex) << 4) + (Get2Bits(Data, pIndex) << 2) + Get2Bits(Data, pIndex) + 29;
               color = Get2Bits(Data, pIndex);
               break;
          }
        }
     }
  DrawLine(x, y, color, rl);
  x += rl;
  return true;
}

bool cSubtitleObject::Decode4BppCodeString(const uchar *Data, int &pIndex, int &x, int y)
{
  int rl = 0;
  int color = 0;
  uchar code = Get4Bits(Data, pIndex);
  if (code) {
     color = code;
     rl = 1;
     }
  else {
     code = Get4Bits(Data, pIndex);
     if (code & 8) { // switch_1
        if (code & 4) { //switch_2
           switch (code & 3) { //switch_3
             case 0: // color 0
                  rl = 1;
                  break;
             case 1: // color 0
                  rl = 2;
                  break;
             case 2:
                  rl = Get4Bits(Data, pIndex) + 9;
                  color = Get4Bits(Data, pIndex);
                  break;
             case 3:
                  rl = (Get4Bits(Data, pIndex) << 4) + Get4Bits(Data, pIndex) + 25;
                  color = Get4Bits(Data, pIndex);
                  break;
             }
           }
        else {
           rl = (code & 3) + 4;
           color = Get4Bits(Data, pIndex);
           }
        }
     else { // color 0
        if (!code)
           return false;
        rl = code + 2;
        }
     }
  DrawLine(x, y, color, rl);
  x += rl;
  return true;
}

bool cSubtitleObject::Decode8BppCodeString(const uchar *Data, int &pIndex, int &x, int y)
{
  int rl = 0;
  int color = 0;
  uchar code = Data[pIndex++];
  if (code) {
     color = code;
     rl = 1;
     }
  else {
     code = Data[pIndex++];
     rl = code & 0x63;
     if (code & 0x80)
        color = Data[pIndex++];
     else if (!rl)
        return false; //else color 0
     }
  DrawLine(x, y, color, rl);
  x += rl;
  return true;
}

// --- cSubtitleRegion -------------------------------------------------------

class cSubtitleRegion : public cListObject, public cBitmap {
private:
  int regionId;
  int version;
  int clutId;
  int horizontalAddress;
  int verticalAddress;
  int level;
  cList<cSubtitleObject> objects;
public:
  cSubtitleRegion(int RegionId);
  int RegionId(void) { return regionId; }
  int Version(void) { return version; }
  int ClutId(void) { return clutId; }
  int Level(void) { return level; }
  int Depth(void) { return Bpp(); }
  void FillRegion(tIndex Index);
  cSubtitleObject *GetObjectById(int ObjectId, bool New = false);
  int HorizontalAddress(void) { return horizontalAddress; }
  int VerticalAddress(void) { return verticalAddress; }
  void SetVersion(int pVersion) { version = pVersion; }
  void SetClutId(int pClutId) { clutId = pClutId; }
  void SetLevel(int Level);
  void SetDepth(int Depth);
  void SetHorizontalAddress(int pHorizontalAddress) { horizontalAddress = pHorizontalAddress; }
  void SetVerticalAddress(int pVerticalAddress) { verticalAddress = pVerticalAddress; }
  };

cSubtitleRegion::cSubtitleRegion(int pRegionId)
:cBitmap(1, 1, 4)
{
  regionId = pRegionId;
  version = -1;
  clutId = -1;
  horizontalAddress = 0;
  verticalAddress = 0;
  level = 0;
}

void cSubtitleRegion::FillRegion(tIndex pIndex)
{
  dbgregions("FillRegion %d\n", pIndex);
  for (int y = 0; y < Height(); y++) {
      for (int x = 0; x < Width(); x++)
          SetIndex(x, y, pIndex);
      }
}

cSubtitleObject *cSubtitleRegion::GetObjectById(int ObjectId, bool New)
{
  cSubtitleObject *result = NULL;
  for (cSubtitleObject *so = objects.First(); so; so = objects.Next(so)) {
      if (so->ObjectId() == ObjectId)
         result = so;
      }
  if (!result && New) {
     result = new cSubtitleObject(ObjectId, this);
     objects.Add(result);
     }
  return result;
}

void cSubtitleRegion::SetLevel(int pLevel)
{
  if (pLevel > 0 && pLevel < 4)
     level = 1 << pLevel;
}

void cSubtitleRegion::SetDepth(int pDepth)
{
  if (pDepth > 0 && pDepth < 4)
     SetBpp(1 << pDepth);
}

// --- cDvbSubtitlePage ------------------------------------------------------

class cDvbSubtitlePage : public cListObject {
private:
  int pageId;
  int version;
  int state;
  int64_t pts;
  int timeout;
  cList<cSubtitleClut> cluts;
public:
  cList<cSubtitleRegion> regions;
  cDvbSubtitlePage(int PageId);
  virtual ~cDvbSubtitlePage();
  int PageId(void) { return pageId; }
  int Version(void) { return version; }
  int State(void) { return state; }
  tArea *GetAreas(void);
  cSubtitleClut *GetClutById(int ClutId, bool New = false);
  cSubtitleObject *GetObjectById(int ObjectId);
  cSubtitleRegion *GetRegionById(int RegionId, bool New = false);
  int64_t Pts(void) const { return pts; }
  int Timeout(void) { return timeout; }
  void SetVersion(int pVersion) { version = pVersion; }
  void SetPts(int64_t pPts) { pts = pPts; }
  void SetState(int State);
  void SetTimeout(int pTimeout) { timeout = pTimeout; }
  void UpdateRegionPalette(cSubtitleClut *Clut);
};

cDvbSubtitlePage::cDvbSubtitlePage(int pPageId)
{
	pageId = pPageId;
	version = -1;
	state = -1;
	pts = 0;
	timeout = 0;
}

cDvbSubtitlePage::~cDvbSubtitlePage()
{
}

tArea *cDvbSubtitlePage::GetAreas(void)
{
  if (regions.Count() > 0) {
     tArea *Areas = new tArea[regions.Count()];
     tArea *a = Areas;
     for (cSubtitleRegion *sr = regions.First(); sr; sr = regions.Next(sr)) {
         a->x1 = sr->HorizontalAddress();
         a->y1 = sr->VerticalAddress();
         a->x2 = sr->HorizontalAddress() + sr->Width() - 1;
         a->y2 = sr->VerticalAddress() + sr->Height() - 1;
         a->bpp = sr->Bpp();
         while ((a->Width() & 3) != 0)
               a->x2++; // aligns width to a multiple of 4, so 2, 4 and 8 bpp will work
         a++;
         }
     return Areas;
     }
  return NULL;
}

cSubtitleClut *cDvbSubtitlePage::GetClutById(int ClutId, bool New)
{
  cSubtitleClut *result = NULL;
  for (cSubtitleClut *sc = cluts.First(); sc; sc = cluts.Next(sc)) {
      if (sc->ClutId() == ClutId)
         result = sc;
      }
  if (!result && New) {
     result = new cSubtitleClut(ClutId);
     cluts.Add(result);
     }
  return result;
}

cSubtitleRegion *cDvbSubtitlePage::GetRegionById(int RegionId, bool New)
{
  cSubtitleRegion *result = NULL;
  for (cSubtitleRegion *sr = regions.First(); sr; sr = regions.Next(sr)) {
      if (sr->RegionId() == RegionId)
         result = sr;
      }
  if (!result && New) {
     result = new cSubtitleRegion(RegionId);
     regions.Add(result);
     }
  return result;
}

cSubtitleObject *cDvbSubtitlePage::GetObjectById(int ObjectId)
{
  cSubtitleObject *result = NULL;
  for (cSubtitleRegion *sr = regions.First(); sr && !result; sr = regions.Next(sr))
      result = sr->GetObjectById(ObjectId);
  return result;
}

void cDvbSubtitlePage::SetState(int pState)
{
  state = pState;
  switch (state) {
    case 0: // normal case - page update
         dbgpages("page update\n");
         break;
    case 1: // aquisition point - page refresh
         dbgpages("page refresh\n");
         regions.Clear();
         break;
    case 2: // mode change - new page
         dbgpages("new Page\n");
         regions.Clear();
         cluts.Clear();
         break;
    case 3: // reserved
         break;
    }
}

void cDvbSubtitlePage::UpdateRegionPalette(cSubtitleClut *Clut)
{
  for (cSubtitleRegion *sr = regions.First(); sr; sr = regions.Next(sr)) {
      if (sr->ClutId() == Clut->ClutId())
         sr->Replace(*Clut->GetPalette(sr->Bpp()));
      }
}

// --- cDvbSubtitleAssembler -------------------------------------------------

class cDvbSubtitleAssembler {
private:
  uchar *data;
  int length;
  int pos;
  int size;
  bool Realloc(int Size);
public:
  cDvbSubtitleAssembler(void);
  virtual ~cDvbSubtitleAssembler();
  void Reset(void);
  unsigned char *Get(int &Length);
  void Put(const uchar *Data, int Length);
  };

cDvbSubtitleAssembler::cDvbSubtitleAssembler(void)
{
  data = NULL;
  size = 0;
  Reset();
}

cDvbSubtitleAssembler::~cDvbSubtitleAssembler()
{
  free(data);
}

void cDvbSubtitleAssembler::Reset(void)
{
  length = 0;
  pos = 0;
}

bool cDvbSubtitleAssembler::Realloc(int Size)
{
  if (Size > size) {
     size = max(Size, 2048);
     data = (uchar *)realloc(data, size);
     if (!data) {
        esyslog("ERROR: can't allocate memory for subtitle assembler");
        length = 0;
        size = 0;
        return false;
        }
     }
  return true;
}

unsigned char *cDvbSubtitleAssembler::Get(int &Length)
{
  if (length > pos + 5) {
     Length = (data[pos + 4] << 8) + data[pos + 5] + 6;
     if (length >= pos + Length) {
        unsigned char *result = data + pos;
        pos += Length;
        return result;
        }
     }
  return NULL;
}

void cDvbSubtitleAssembler::Put(const uchar *Data, int Length)
{
  if (Length && Realloc(length + Length)) {
     memcpy(data + length, Data, Length);
     length += Length;
     }
}

// --- cDvbSubtitleBitmaps ---------------------------------------------------

class cDvbSubtitleBitmaps : public cListObject {
private:
  int64_t pts;
  int timeout;
  tArea *areas;
  int numAreas;
  cVector<cBitmap *> bitmaps;
//  int min_x, min_y, max_x, max_y;
public:
  cDvbSubtitleBitmaps(int64_t Pts, int Timeout, tArea *Areas, int NumAreas);
  ~cDvbSubtitleBitmaps();
  int64_t Pts(void) { return pts; }
  int Timeout(void) { return timeout; }
  void AddBitmap(cBitmap *Bitmap);
  void Draw();
  void Clear(void);
  };

cDvbSubtitleBitmaps::cDvbSubtitleBitmaps(int64_t pPts, int pTimeout, tArea *pAreas, int pNumAreas)
{
  pts = pPts;
  timeout = pTimeout;
  areas = pAreas;
  numAreas = pNumAreas;
//  max_x = max_y = 0;
//  min_x = min_y = 0xFFFF;
  //dbgconverter("cDvbSubtitleBitmaps::new: PTS: %lld\n", pts);
}

cDvbSubtitleBitmaps::~cDvbSubtitleBitmaps()
{
  //dbgconverter("cDvbSubtitleBitmaps::delete: PTS: %lld\n", pts);
  delete[] areas;
  for (int i = 0; i < bitmaps.Size(); i++)
      delete bitmaps[i];
}

void cDvbSubtitleBitmaps::AddBitmap(cBitmap *Bitmap)
{
  bitmaps.Append(Bitmap);
  //dbgconverter("cDvbSubtitleBitmaps::AddBitmap size %d PTS: %lld\n", bitmaps.Size(), pts);
}

static int min_x = 0xFFFF, min_y = 0xFFFF;
static int max_x = 0, max_y = 0;

void cDvbSubtitleBitmaps::Clear()
{
	dbgconverter("cDvbSubtitleBitmaps::Clear: x=% d y= %d, w= %d, h= %d\n", min_x, min_y, max_x-min_x, max_y-min_y);
	if(max_x && max_y) {
		//CFrameBuffer::getInstance()->paintBackgroundBoxRel (min_x, min_y-10, max_x-min_x, max_y-min_y+10);
		CFrameBuffer::getInstance()->paintBackground();
		max_x = max_y = 0;
		min_x = min_y = 0xFFFF;
	}
}

void cDvbSubtitleBitmaps::Draw()
{
	static tColor save_colors[MAXNUMCOLORS];

	int stride = CFrameBuffer::getInstance()->getScreenWidth(true);
	int wd = CFrameBuffer::getInstance()->getScreenWidth();
	int xs = CFrameBuffer::getInstance()->getScreenX();
	int yend = CFrameBuffer::getInstance()->getScreenY() + CFrameBuffer::getInstance()->getScreenHeight();
	uint32_t *sublfb = CFrameBuffer::getInstance()->getFrameBufferPointer();

	dbgconverter("cDvbSubtitleBitmaps::Draw: %d bitmaps, x= %d, width= %d end=%d stride %d\n", bitmaps.Size(), xs, wd, yend, stride);

	if(bitmaps.Size())
		Clear(); //FIXME should we clear for new bitmaps set ?

	for (int i = 0; i < bitmaps.Size(); i++) {

		int NumColors;
		const tColor *Colors = bitmaps[i]->Colors(NumColors);
		if (Colors) {
			memcpy(save_colors, Colors, sizeof(tColor)*NumColors);
		}
		/* center on screen */
		int xoff = xs + (wd - bitmaps[i]->Width())/2;
		/* move to screen bottom */
		int yoff = (yend - (576-bitmaps[i]->Y0()))*stride;
		int ys = yend - (576-bitmaps[i]->Y0());

		dbgconverter("cDvbSubtitleBitmaps::Draw num %d colors= %d at %d,%d (x=%d y=%d) size %dx%d\n",
			i, NumColors, bitmaps[i]->X0(), bitmaps[i]->Y0(), xoff, ys, bitmaps[i]->Width(), bitmaps[i]->Height());

		for (int y2 = 0; y2 < bitmaps[i]->Height(); y2++) {
			int y = y2*stride + yoff;
			for (int x2 = 0; x2 < bitmaps[i]->Width(); x2++)
			{
				int idx = *(bitmaps[i]->Data(x2, y2));
				*(sublfb + xoff + x2 + y) = save_colors[idx];
			}
		}
		if(min_x > xoff)
			min_x = xoff;
		if(min_y > ys)
			min_y = ys;
		if(max_x < (xoff + bitmaps[i]->Width()))
			max_x = xoff + bitmaps[i]->Width();
		if(max_y < (ys+bitmaps[i]->Height()))
			max_y = ys+bitmaps[i]->Height();
	}
	if(bitmaps.Size())
		dbgconverter("cDvbSubtitleBitmaps::Draw: finish, min/max screen: x=% d y= %d, w= %d, h= %d\n", min_x, min_y, max_x-min_x, max_y-min_y);
}

// --- cDvbSubtitleConverter -------------------------------------------------

int cDvbSubtitleConverter::setupLevel = 0;

cDvbSubtitleConverter::cDvbSubtitleConverter(void)
//:cThread("subtitleConverter")
{
  dvbSubtitleAssembler = new cDvbSubtitleAssembler;
  pages = new cList<cDvbSubtitlePage>;
  bitmaps = new cList<cDvbSubtitleBitmaps>;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
  pthread_mutex_init(&mutex, &attr);
  running = false;
//  Start();
}

cDvbSubtitleConverter::~cDvbSubtitleConverter()
{
//  Cancel(3);
  delete dvbSubtitleAssembler;
  delete bitmaps;
  delete pages;
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
		//CFrameBuffer::getInstance()->paintBackgroundBoxRel (min_x, min_y-10, max_x-min_x, max_y-min_y+10);
		CFrameBuffer::getInstance()->paintBackground();
		max_x = max_y = 0;
		min_x = min_y = 0xFFFF;
	}
}

void cDvbSubtitleConverter::SetupChanged(void)
{
  setupLevel++;
}

void cDvbSubtitleConverter::Reset(void)
{
  dbgconverter("Converter reset -----------------------\n");
  dvbSubtitleAssembler->Reset();
  Lock();
  pages->Clear();
  bitmaps->Clear();
  Unlock();
}

int cDvbSubtitleConverter::ConvertFragments(const uchar *Data, int Length, int64_t pts)
{
  if (Data && Length > 8) {
     int PayloadOffset = 0;//PesPayloadOffset(Data);
     int SubstreamHeaderLength = 4;
     bool ResetSubtitleAssembler = Data[PayloadOffset + 3] == 0x00;

     if (Length > PayloadOffset + SubstreamHeaderLength) {
        const uchar *data = Data + PayloadOffset + SubstreamHeaderLength; // skip substream header
        int length = Length - PayloadOffset - SubstreamHeaderLength; // skip substream header
        if (ResetSubtitleAssembler)
           dvbSubtitleAssembler->Reset();

        if (length > 3) {
           if (data[0] == 0x20 && data[1] == 0x00 && data[2] == 0x0F)
              dvbSubtitleAssembler->Put(data + 2, length - 2);
           else
              dvbSubtitleAssembler->Put(data, length);

           int Count;
           while (true) {
                 unsigned char *b = dvbSubtitleAssembler->Get(Count);
                 if (b && b[0] == 0x0F) {
                    if (ExtractSegment(b, Count, pts) == -1)
                       break;
                    }
                 else {
                    break;
                 }
                 }
           }
        }
     return Length;
     }
  return 0;
}

int cDvbSubtitleConverter::Convert(const uchar *Data, int Length, int64_t pts)
{
  if (Data && Length > 8) {
     int PayloadOffset = 0;//PesPayloadOffset(Data);
     if (Length > PayloadOffset) {
        const uchar *data = Data + PayloadOffset;
        int length = Length - PayloadOffset;
        if (length > 3) {
           if (data[0] == 0x20 && data[1] == 0x00 && data[2] == 0x0F) {
              data += 2;
              length -= 2;
              }
           const uchar *b = data;
           while (length > 0) {
                 if (b[0] == 0x0F) {
                    int n = ExtractSegment(b, length, pts);
                    if (n < 0)
                       break;
                    b += n;
                    length -= n;
                    }
                 else
                    break;
                 }
           }
        }
     return Length;
     }
  return 0;
}

#define LimitTo32Bit(n) (n & 0x00000000FFFFFFFFL)
#define MAXDELTA 40000 // max. reasonable PTS/STC delta in ms

void dvbsub_get_stc(int64_t * STC);

int cDvbSubtitleConverter::Action(void)
{
	static cTimeMs Timeout(0xFFFF*1000);
	int WaitMs = 500;

	if(!running)
		return 0;

	Lock();
	if (cDvbSubtitleBitmaps *sb = bitmaps->First()) {
		int64_t STC;
		dvbsub_get_stc(&STC);
		int64_t Delta = 0;

		Delta = LimitTo32Bit(sb->Pts()) - LimitTo32Bit(STC);
		Delta /= 90; // STC and PTS are in 1/90000s
		dbgconverter("cDvbSubtitleConverter::Action: PTS: %lld  STC: %lld (%lld) timeout: %d\n", sb->Pts(), STC, Delta, sb->Timeout());

#if 0
		if(Delta > 1800) {
			Unlock();
			usleep((Delta-500)*1000);
			Lock();
			if(!running) {
				Unlock();
				return 0;
			}
#if 1 // debug
			dvbsub_get_stc(&STC);
			Delta = sb->Pts() - STC;
			dbgconverter("cDvbSubtitleConverter::Action: PTS: %lld  STC: %lld (%lld) timeout: %d after sleep\n", sb->Pts(), STC, Delta/90, sb->Timeout());
#endif
		}
#endif
		if (Delta <= MAXDELTA) {
			if (Delta <= 0) {
				dbgconverter("cDvbSubtitleConverter::Action: Got %d bitmaps, showing #%d\n", bitmaps->Count(), sb->Index() + 1);
				if (running) {
					sb->Draw();
					Timeout.Set(sb->Timeout() * 1000);//max: was 1000 and timeout seems in 1/10 of sec ??
				}
				bitmaps->Del(sb, true);
				WaitMs = 1000;
			}
			else if (Delta < WaitMs)
				WaitMs = Delta;
		}
		else
			bitmaps->Del(sb, true);
	} else {
		//printf("cDvbSubtitleConverter::Action: timeout elapsed %lld\n", Timeout.Elapsed());
		if (Timeout.TimedOut()) {
			dbgconverter("cDvbSubtitleConverter::Action: timeout, elapsed %lld\n", Timeout.Elapsed());
			Clear();
			Timeout.Set(0xFFFF*1000);
		}
	}
	Unlock();
	dbgconverter("cDvbSubtitleConverter::Action: finish, WaitMs %d\n", WaitMs);
	return WaitMs*1000;
}

tColor cDvbSubtitleConverter::yuv2rgb(int Y, int Cb, int Cr)
{
  int Ey, Epb, Epr;
  int Eg, Eb, Er;

  Ey = (Y - 16);
  Epb = (Cb - 128);
  Epr = (Cr - 128);
  /* ITU-R 709 */
  Er = max(min(((298 * Ey             + 460 * Epr) / 256), 255), 0);
  Eg = max(min(((298 * Ey -  55 * Epb - 137 * Epr) / 256), 255), 0);
  Eb = max(min(((298 * Ey + 543 * Epb            ) / 256), 255), 0);

  return (Er << 16) | (Eg << 8) | Eb;
}

bool cDvbSubtitleConverter::AssertOsd(void)
{
  return 1;//osd || (osd = cOsdProvider::NewOsd(0, Setup.SubtitleOffset, OSD_LEVEL_SUBTITLES));
}

int cDvbSubtitleConverter::ExtractSegment(const uchar *Data, int Length, int64_t Pts)
{
  if (Length > 5 && Data[0] == 0x0F) {
     int segmentLength = (Data[4] << 8) + Data[5] + 6;
     if (segmentLength > Length)
        return -1;
     int segmentType = Data[1];
     int pageId = (Data[2] << 8) + Data[3];
     cDvbSubtitlePage *page = NULL;
//     LOCK_THREAD;
     for (cDvbSubtitlePage *sp = pages->First(); sp; sp = pages->Next(sp)) {
         if (sp->PageId() == pageId) {
            page = sp;
            break;
            }
         }
     if (!page) {
        page = new cDvbSubtitlePage(pageId);
        pages->Add(page);
        dbgpages("Create SubtitlePage %d  (total pages = %d)\n", pageId, pages->Count());
        }
     if (Pts)
        page->SetPts(Pts);
     switch (segmentType) {
       case PAGE_COMPOSITION_SEGMENT: {
            dbgsegments("PAGE_COMPOSITION_SEGMENT\n");
            int pageVersion = (Data[6 + 1] & 0xF0) >> 4;
            if (pageVersion == page->Version())
               break; // no update
            page->SetVersion(pageVersion);
            page->SetTimeout(Data[6]);
            page->SetState((Data[6 + 1] & 0x0C) >> 2);
            page->regions.Clear();
            dbgpages("Update page id %d version %d pts %lld timeout %d state %d\n", pageId, page->Version(), page->Pts(), page->Timeout(), page->State());
            for (int i = 6 + 2; i < segmentLength; i += 6) {
                cSubtitleRegion *region = page->GetRegionById(Data[i], true);
                region->SetHorizontalAddress((Data[i + 2] << 8) + Data[i + 3]);
                region->SetVerticalAddress((Data[i + 4] << 8) + Data[i + 5]);
                }
            break;
            }
       case REGION_COMPOSITION_SEGMENT: {
            dbgsegments("REGION_COMPOSITION_SEGMENT\n");
            cSubtitleRegion *region = page->GetRegionById(Data[6]);
            if (!region)
               break;
            int regionVersion = (Data[6 + 1] & 0xF0) >> 4;
            if (regionVersion == region->Version())
               break; // no update
            region->SetVersion(regionVersion);
            bool regionFillFlag = (Data[6 + 1] & 0x08) >> 3;
            int regionWidth = (Data[6 + 2] << 8) | Data[6 + 3];
            int regionHeight = (Data[6 + 4] << 8) | Data[6 + 5];
            region->SetSize(regionWidth, regionHeight);
            region->SetLevel((Data[6 + 6] & 0xE0) >> 5);
            region->SetDepth((Data[6 + 6] & 0x1C) >> 2);
            region->SetClutId(Data[6 + 7]);
            dbgregions("Region pageId %d id %d version %d fill %d width %d height %d level %d depth %d clutId %d\n", pageId, region->RegionId(), region->Version(), regionFillFlag, regionWidth, regionHeight, region->Level(), region->Depth(), region->ClutId());
            if (regionFillFlag) {
               switch (region->Bpp()) {
                 case 2: region->FillRegion((Data[6 + 9] & 0x0C) >> 2); break;
                 case 4: region->FillRegion((Data[6 + 9] & 0xF0) >> 4); break;
                 case 8: region->FillRegion(Data[6 + 8]); break;
                 }
               }
            for (int i = 6 + 10; i < segmentLength; i += 6) {
                cSubtitleObject *object = region->GetObjectById((Data[i] << 8) | Data[i + 1], true);
                int objectType = (Data[i + 2] & 0xC0) >> 6;
                object->SetCodingMethod(objectType);
                object->SetProviderFlag((Data[i + 2] & 0x30) >> 4);
                int objectHorizontalPosition = ((Data[i + 2] & 0x0F) << 8) | Data[i + 3];
                int objectVerticalPosition = ((Data[i + 4] & 0x0F) << 8) | Data[i + 5];
                object->SetPosition(objectHorizontalPosition, objectVerticalPosition);
                if (objectType == 0x01 || objectType == 0x02) {
                   object->SetForegroundColor(Data[i + 6]);
                   object->SetBackgroundColor(Data[i + 7]);
                   i += 2;
                   }
                }
            break;
            }
       case CLUT_DEFINITION_SEGMENT: {
            dbgsegments("CLUT_DEFINITION_SEGMENT\n");
            cSubtitleClut *clut =  page->GetClutById(Data[6], true);
            int clutVersion = (Data[6 + 1] & 0xF0) >> 4;
            if (clutVersion == clut->Version())
               break; // no update
            clut->SetVersion(clutVersion);
            dbgcluts("Clut pageId %d id %d version %d\n", pageId, clut->ClutId(), clut->Version());
            for (int i = 6 + 2; i < segmentLength; i += 2) {
                uchar clutEntryId = Data[i];
                bool fullRangeFlag = Data[i + 1] & 1;
                uchar yval;
                uchar crval;
                uchar cbval;
                uchar tval;
                if (fullRangeFlag) {
                   yval  = Data[i + 2];
                   crval = Data[i + 3];
                   cbval = Data[i + 4];
                   tval  = Data[i + 5];
                   }
                else {
                   yval   =   Data[i + 2] & 0xFC;
                   crval  =  (Data[i + 2] & 0x03) << 6;
                   crval |=  (Data[i + 3] & 0xC0) >> 2;
                   cbval  =  (Data[i + 3] & 0x3C) << 2;
                   tval   =  (Data[i + 3] & 0x03) << 6;
                   }
                tColor value = 0;
                if (yval) {
                   value = yuv2rgb(yval, cbval, crval);
                   value |= ((10 - (clutEntryId ? SubtitleFgTransparency : SubtitleBgTransparency)) * (255 - tval) / 10) << 24;
                   }
                int EntryFlags = Data[i + 1];
                dbgcluts("%2d %d %d %d %08X\n", clutEntryId, (EntryFlags & 0x80) ? 2 : 0, (EntryFlags & 0x40) ? 4 : 0, (EntryFlags & 0x20) ? 8 : 0, value);
                if ((EntryFlags & 0x80) != 0)
                   clut->SetColor(2, clutEntryId, value);
                if ((EntryFlags & 0x40) != 0)
                   clut->SetColor(4, clutEntryId, value);
                if ((EntryFlags & 0x20) != 0)
                   clut->SetColor(8, clutEntryId, value);
                i += fullRangeFlag ? 4 : 2;
                }
            dbgcluts("\n");
            page->UpdateRegionPalette(clut);
            break;
            }
       case OBJECT_DATA_SEGMENT: {
            dbgsegments("OBJECT_DATA_SEGMENT\n");
            cSubtitleObject *object = page->GetObjectById((Data[6] << 8) | Data[6 + 1]);
            if (!object)
               break;
            int objectVersion = (Data[6 + 2] & 0xF0) >> 4;
            if (objectVersion == object->Version())
               break; // no update
            object->SetVersion(objectVersion);
            int codingMethod = (Data[6 + 2] & 0x0C) >> 2;
            object->SetNonModifyingColorFlag(Data[6 + 2] & 0x01);
            dbgobjects("Object pageId %d id %d version %d method %d modify %d\n", pageId, object->ObjectId(), object->Version(), object->CodingMethod(), object->NonModifyingColorFlag());
            if (codingMethod == 0) { // coding of pixels
               int i = 6 + 3;
               int topFieldLength = (Data[i] << 8) | Data[i + 1];
               int bottomFieldLength = (Data[i + 2] << 8) | Data[i + 3];
               object->DecodeSubBlock(Data + i + 4, topFieldLength, true);
               if (bottomFieldLength)
                  object->DecodeSubBlock(Data + i + 4 + topFieldLength, bottomFieldLength, false);
               else
                  object->DecodeSubBlock(Data + i + 4, topFieldLength, false);
               }
            else if (codingMethod == 1) { // coded as a string of characters
               //TODO implement textual subtitles
               }
            break;
            }
       case END_OF_DISPLAY_SET_SEGMENT: {
            dbgsegments("END_OF_DISPLAY_SET_SEGMENT\n");
            FinishPage(page);
            }
            break;
       default:
            dbgsegments("*** unknown segment type: %02X\n", segmentType);
       }
     return segmentLength;
     }
  return -1;
}

void cDvbSubtitleConverter::FinishPage(cDvbSubtitlePage *Page)
{
  if (!AssertOsd())
     return;
  tArea *Areas = Page->GetAreas();
  int NumAreas = Page->regions.Count();
#if 0
  int Bpp = 8;
  bool Reduced = false;

  while (osd->CanHandleAreas(Areas, NumAreas) != oeOk) {
        int HalfBpp = Bpp / 2;
        if (HalfBpp >= 2) {
           for (int i = 0; i < NumAreas; i++) {
               if (Areas[i].bpp >= Bpp) {
                  Areas[i].bpp = HalfBpp;
                  Reduced = true;
                  }
               }
           Bpp = HalfBpp;
           }
        else
           return; // unable to draw bitmaps
        }

  if (Reduced) {
     for (int i = 0; i < NumAreas; i++) {
         cSubtitleRegion *sr = Page->regions.Get(i);
         if (sr->Bpp() != Areas[i].bpp) {
            if (sr->Level() <= Areas[i].bpp) {
               //TODO this is untested - didn't have any such subtitle stream
               cSubtitleClut *Clut = Page->GetClutById(sr->ClutId());
               if (Clut) {
                  dbgregions("reduce region %d bpp %d level %d area bpp %d\n", sr->RegionId(), sr->Bpp(), sr->Level(), Areas[i].bpp);
                  sr->ReduceBpp(*Clut->GetPalette(sr->Bpp()));
                  }
               }
            else {
               dbgregions("condense region %d bpp %d level %d area bpp %d\n", sr->RegionId(), sr->Bpp(), sr->Level(), Areas[i].bpp);
               sr->ShrinkBpp(Areas[i].bpp);
               }
            }
         }
     }
#endif
  cDvbSubtitleBitmaps *Bitmaps = new cDvbSubtitleBitmaps(Page->Pts(), Page->Timeout(), Areas, NumAreas);
  bitmaps->Add(Bitmaps);
  for (cSubtitleRegion *sr = Page->regions.First(); sr; sr = Page->regions.Next(sr)) {
      int posX = sr->HorizontalAddress();
      int posY = sr->VerticalAddress();
      cBitmap *bm = new cBitmap(sr->Width(), sr->Height(), sr->Bpp(), posX, posY);
      bm->DrawBitmap(posX, posY, *sr);
      Bitmaps->AddBitmap(bm);
      }
}
