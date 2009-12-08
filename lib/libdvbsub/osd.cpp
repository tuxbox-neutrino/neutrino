/*
 * osd.c: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.cpp,v 1.1 2009/02/23 19:46:44 rhabarber1848 Exp $
 */

#include "dvbsubtitle.h"
//#include "device.h"

#define PAGE_COMPOSITION_SEGMENT   0x10
#define REGION_COMPOSITION_SEGMENT 0x11
#define CLUT_DEFINITION_SEGMENT    0x12
#define OBJECT_DATA_SEGMENT        0x13
#define END_OF_DISPLAY_SET_SEGMENT 0x80

// --- cPalette --------------------------------------------------------------

cPalette::cPalette(int Bpp)
{
  SetBpp(Bpp);
  SetAntiAliasGranularity(10, 10);
}

void cPalette::SetAntiAliasGranularity(uint FixedColors, uint BlendColors)
{
  if (FixedColors >= MAXNUMCOLORS || BlendColors == 0)
     antiAliasGranularity = MAXNUMCOLORS - 1;
  else {
     int ColorsForBlending = MAXNUMCOLORS - FixedColors;
     int ColorsPerBlend = ColorsForBlending / BlendColors + 2; // +2 = the full foreground and background colors, which are amoung the fixed colors
     antiAliasGranularity = double(MAXNUMCOLORS - 1) / (ColorsPerBlend - 1);
     }
}

void cPalette::Reset(void)
{
  numColors = 0;
  modified = false;
}

int cPalette::Index(tColor Color)
{
  // Check if color is already defined:
  for (int i = 0; i < numColors; i++) {
      if (color[i] == Color)
         return i;
      }
  // No exact color, try a close one:
  int i = ClosestColor(Color, 4);
  if (i >= 0)
     return i;
  // No close one, try to define a new one:
  if (numColors < maxColors) {
     color[numColors++] = Color;
     modified = true;
     return numColors - 1;
     }
  // Out of colors, so any close color must do:
  return ClosestColor(Color);
}

void cPalette::SetBpp(int Bpp)
{
  bpp = Bpp;
  maxColors = 1 << bpp;
  Reset();
}

void cPalette::SetColor(int Index, tColor Color)
{
  if (Index < maxColors) {
     if (numColors <= Index) {
        numColors = Index + 1;
        modified = true;
        }
     else
        modified |= color[Index] != Color;
     color[Index] = Color;
     }
}

const tColor *cPalette::Colors(int &NumColors) const
{
  NumColors = numColors;
  return numColors ? color : NULL;
}

void cPalette::Take(const cPalette &Palette, tIndexes *Indexes, tColor ColorFg, tColor ColorBg)
{
  for (int i = 0; i < Palette.numColors; i++) {
      tColor Color = Palette.color[i];
      if (ColorFg || ColorBg) {
         switch (i) {
           case 0: Color = ColorBg; break;
           case 1: Color = ColorFg; break;
           }
         }
      int n = Index(Color);
      if (Indexes)
         (*Indexes)[i] = n;
      }
}

void cPalette::Replace(const cPalette &Palette)
{
  for (int i = 0; i < Palette.numColors; i++)
      SetColor(i, Palette.color[i]);
  numColors = Palette.numColors;
  antiAliasGranularity = Palette.antiAliasGranularity;
}

tColor cPalette::Blend(tColor ColorFg, tColor ColorBg, uint8_t Level) const
{
  if (antiAliasGranularity > 0)
     Level = uint8_t(int(Level / antiAliasGranularity + 0.5) * antiAliasGranularity);
  int Af = (ColorFg & 0xFF000000) >> 24;
  int Rf = (ColorFg & 0x00FF0000) >> 16;
  int Gf = (ColorFg & 0x0000FF00) >>  8;
  int Bf = (ColorFg & 0x000000FF);
  int Ab = (ColorBg & 0xFF000000) >> 24;
  int Rb = (ColorBg & 0x00FF0000) >> 16;
  int Gb = (ColorBg & 0x0000FF00) >>  8;
  int Bb = (ColorBg & 0x000000FF);
  int A = (Ab + (Af - Ab) * Level / 0xFF) & 0xFF;
  int R = (Rb + (Rf - Rb) * Level / 0xFF) & 0xFF;
  int G = (Gb + (Gf - Gb) * Level / 0xFF) & 0xFF;
  int B = (Bb + (Bf - Bb) * Level / 0xFF) & 0xFF;
  return (A << 24) | (R << 16) | (G << 8) | B;
}

int cPalette::ClosestColor(tColor Color, int MaxDiff) const
{
  int n = 0;
  int d = INT_MAX;
  int A1 = (Color & 0xFF000000) >> 24;
  int R1 = (Color & 0x00FF0000) >> 16;
  int G1 = (Color & 0x0000FF00) >>  8;
  int B1 = (Color & 0x000000FF);
  for (int i = 0; i < numColors; i++) {
      int A2 = (color[i] & 0xFF000000) >> 24;
      int R2 = (color[i] & 0x00FF0000) >> 16;
      int G2 = (color[i] & 0x0000FF00) >>  8;
      int B2 = (color[i] & 0x000000FF);
      int diff = (abs(A1 - A2) << 1) + (abs(R1 - R2) << 1) + (abs(G1 - G2) << 1) + (abs(B1 - B2) << 1);
      if (diff < d) {
         d = diff;
         n = i;
         }
      }
  return d <= MaxDiff ? n : -1;
}

cBitmap::cBitmap(int Width, int Height, int Bpp, int X0, int Y0)
:cPalette(Bpp)
{
  bitmap = NULL;
  x0 = X0;
  y0 = Y0;
  SetSize(Width, Height);
}

cBitmap::~cBitmap()
{
  free(bitmap);
}

void cBitmap::SetIndex(int x, int y, tIndex Index)
{
  if (bitmap) {
     if (0 <= x && x < width && 0 <= y && y < height) {
        if (bitmap[width * y + x] != Index) {
           bitmap[width * y + x] = Index;
           if (dirtyX1 > x)  dirtyX1 = x;
           if (dirtyY1 > y)  dirtyY1 = y;
           if (dirtyX2 < x)  dirtyX2 = x;
           if (dirtyY2 < y)  dirtyY2 = y;
           }
        }
     }
}

void cBitmap::SetSize(int Width, int Height)
{
  if (bitmap && Width == width && Height == height)
     return;
  width = Width;
  height = Height;
  free(bitmap);
  bitmap = NULL;
  dirtyX1 = 0;
  dirtyY1 = 0;
  dirtyX2 = width - 1;
  dirtyY2 = height - 1;
  if (width > 0 && height > 0) {
     bitmap = MALLOC(tIndex, width * height);
     if (bitmap)
        memset(bitmap, 0x00, width * height);
     else
        esyslog("ERROR: can't allocate bitmap!");
     }
  else
     esyslog("ERROR: invalid bitmap parameters (%d, %d)!", width, height);
}

void cBitmap::DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg, tColor ColorBg, bool ReplacePalette, bool Overlay)
{
  if (bitmap && Bitmap.bitmap && Intersects(x, y, x + Bitmap.Width() - 1, y + Bitmap.Height() - 1)) {
     if (Covers(x, y, x + Bitmap.Width() - 1, y + Bitmap.Height() - 1))
        Reset();
     x -= x0;
     y -= y0;
     if (ReplacePalette && Covers(x + x0, y + y0, x + x0 + Bitmap.Width() - 1, y + y0 + Bitmap.Height() - 1)) {
        Replace(Bitmap);
        for (int ix = 0; ix < Bitmap.width; ix++) {
            for (int iy = 0; iy < Bitmap.height; iy++) {
                if (!Overlay || Bitmap.bitmap[Bitmap.width * iy + ix] != 0)
                   SetIndex(x + ix, y + iy, Bitmap.bitmap[Bitmap.width * iy + ix]);
                }
            }
        }
     else {
        tIndexes Indexes;
        Take(Bitmap, &Indexes, ColorFg, ColorBg);
        for (int ix = 0; ix < Bitmap.width; ix++) {
            for (int iy = 0; iy < Bitmap.height; iy++) {
                if (!Overlay || Bitmap.bitmap[Bitmap.width * iy + ix] != 0)
                   SetIndex(x + ix, y + iy, Indexes[int(Bitmap.bitmap[Bitmap.width * iy + ix])]);
                }
            }
        }
     }
}


bool cBitmap::Contains(int x, int y) const
{
  x -= x0;
  y -= y0;
  return 0 <= x && x < width && 0 <= y && y < height;
}

bool cBitmap::Covers(int x1, int y1, int x2, int y2) const
{
  x1 -= x0;
  y1 -= y0;
  x2 -= x0;
  y2 -= y0;
  return x1 <= 0 && y1 <= 0 && x2 >= width - 1 && y2 >= height - 1;
}

bool cBitmap::Intersects(int x1, int y1, int x2, int y2) const
{
  x1 -= x0;
  y1 -= y0;
  x2 -= x0;
  y2 -= y0;
  return !(x2 < 0 || x1 >= width || y2 < 0 || y1 >= height);
}

bool cBitmap::Dirty(int &x1, int &y1, int &x2, int &y2)
{
  if (dirtyX2 >= 0) {
     x1 = dirtyX1;
     y1 = dirtyY1;
     x2 = dirtyX2;
     y2 = dirtyY2;
     return true;
     }
  return false;
}

void cBitmap::Clean(void)
{
  dirtyX1 = width;
  dirtyY1 = height;
  dirtyX2 = -1;
  dirtyY2 = -1;
}

const tIndex *cBitmap::Data(int x, int y)
{
  return &bitmap[y * width + x];
}

