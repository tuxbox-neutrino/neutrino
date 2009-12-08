/*
 * osd.h: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original author: Marco Schlüßler <marco@lordzodiac.de>
 *
 * $Id: osd.h,v 1.1 2009/02/23 19:46:44 rhabarber1848 Exp $
 */

#ifndef __OSD_H
#define __OSD_H

#include "tools.h"
#include <limits.h>

#define MAXNUMCOLORS 256

typedef uint32_t tColor;
typedef unsigned char tIndex;

class cPalette {
private:
  tColor color[MAXNUMCOLORS];
  int bpp;
  int maxColors, numColors;
  bool modified;
  double antiAliasGranularity;
protected:
  typedef tIndex tIndexes[MAXNUMCOLORS];
public:
  cPalette(int Bpp = 8);
        ///< Initializes the palette with the given color depth.
  void SetAntiAliasGranularity(uint FixedColors, uint BlendColors);
        ///< Allows the system to optimize utilization of the limited color
        ///< palette entries when generating blended colors for anti-aliasing.
        ///< FixedColors is the maximum number of colors used, and BlendColors
        ///< is the maximum number of foreground/background color combinations
        ///< used with anti-aliasing. If this function is not called with
        ///< useful values, the palette may be filled up with many shades of
        ///< a single color combination, and may not be able to serve all
        ///< requested colors. By default the palette assumes there will be
        ///< 10 fixed colors and 10 color combinations.
  int Bpp(void) const { return bpp; }
  void Reset(void);
        ///< Resets the palette, making it contain no colors.
  int Index(tColor Color);
        ///< Returns the index of the given Color (the first color has index 0).
        ///< If Color is not yet contained in this palette, it will be added if
        ///< there is a free slot. If the color can't be added to this palette,
        ///< the closest existing color will be returned.
  tColor Color(int Index) const { return Index < maxColors ? color[Index] : 0; }
        ///< Returns the color at the given Index. If Index is outside the valid
        ///< range, 0 will be returned.
  void SetBpp(int Bpp);
        ///< Sets the color depth of this palette to the given value.
        ///< The palette contents will be reset, so that it contains no colors.
  void SetColor(int Index, tColor Color);
        ///< Sets the palette entry at Index to Color. If Index is larger than
        ///< the number of currently used entries in this palette, the entries
        ///< in between will have undefined values.
  const tColor *Colors(int &NumColors) const;
        ///< Returns a pointer to the complete color table and stores the
        ///< number of valid entries in NumColors. If no colors have been
        ///< stored yet, NumColors will be set to 0 and the function will
        ///< return NULL.
  void Take(const cPalette &Palette, tIndexes *Indexes = NULL, tColor ColorFg = 0, tColor ColorBg = 0);
        ///< Takes the colors from the given Palette and adds them to this palette,
        ///< using existing entries if possible. If Indexes is given, it will be
        ///< filled with the index values that each color of Palette has in this
        ///< palette. If either of ColorFg or ColorBg is not zero, the first color
        ///< in Palette will be taken as ColorBg, and the second color will become
        ///< ColorFg.
  void Replace(const cPalette &Palette);
        ///< Replaces the colors of this palette with the colors from the given
        ///< palette.
  tColor Blend(tColor ColorFg, tColor ColorBg, uint8_t Level) const;
        ///< Determines a color that consists of a linear blend between ColorFg
        ///< and ColorBg. If Level is 0, the result is ColorBg, if it is 255,
        ///< the result is ColorFg. If SetAntiAliasGranularity() has been called previously,
        ///< Level will be mapped to a limited range of levels that allow to make best
        ///< use of the palette entries.
  int ClosestColor(tColor Color, int MaxDiff = INT_MAX) const;
        ///< Returns the index of a color in this palette that is closest to the given
        ///< Color. MaxDiff can be used to control the maximum allowed color difference.
        ///< If no color with a maximum difference of MaxDiff can be found, -1 will
        ///< be returned. With the default value of INT_MAX, there will always be
        ///< a valid color index returned, but the color may be completely different.
};

class cBitmap : public cPalette {
private:
  tIndex *bitmap;
  int x0, y0;
  int width, height;
  int dirtyX1, dirtyY1, dirtyX2, dirtyY2;
public:
  cBitmap(int Width, int Height, int Bpp, int X0 = 0, int Y0 = 0);
       ///< Creates a bitmap with the given Width, Height and color depth (Bpp).
       ///< X0 and Y0 define the offset at which this bitmap will be located on the OSD.
       ///< All coordinates given in the other functions will be relative to
       ///< this offset (unless specified otherwise).
  cBitmap(const char *FileName);
       ///< Creates a bitmap and loads an XPM image from the given file.
  virtual ~cBitmap();
  int X0(void) const { return x0; }
  int Y0(void) const { return y0; }
  int Width(void) const { return width; }
  int Height(void) const { return height; }
  void SetSize(int Width, int Height);
       ///< Sets the size of this bitmap to the given values. Any previous
       ///< contents of the bitmap will be lost. If Width and Height are the same
       ///< as the current values, nothing will happen and the bitmap remains
       ///< unchanged.
  void SetIndex(int x, int y, tIndex Index);
       ///< Sets the index at the given coordinates to Index.
       ///< Coordinates are relative to the bitmap's origin.
  void DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool ReplacePalette = false, bool Overlay = false);
       ///< Sets the pixels in this bitmap with the data from the given
       ///< Bitmap, putting the upper left corner of the Bitmap at (x, y).
       ///< If ColorFg or ColorBg is given, the first palette entry of the Bitmap
       ///< will be mapped to ColorBg and the second palette entry will be mapped to
       ///< ColorFg (palette indexes are defined so that 0 is the background and
       ///< 1 is the foreground color). ReplacePalette controls whether the target
       ///< area shall have its palette replaced with the one from Bitmap.
       ///< If Overlay is true, any pixel in Bitmap that has color index 0 will
       ///< not overwrite the corresponding pixel in the target area.
  const tIndex *Data(int x, int y);
       ///< Returns the address of the index byte at the given coordinates.

  bool Contains(int x, int y) const;
  bool Covers(int x1, int y1, int x2, int y2) const;
  bool Intersects(int x1, int y1, int x2, int y2) const;
  bool Dirty(int &x1, int &y1, int &x2, int &y2);
  void Clean(void);
};

struct tArea {
  int x1, y1, x2, y2;
  int bpp;
  int Width(void) const { return x2 - x1 + 1; }
  int Height(void) const { return y2 - y1 + 1; }
  bool Intersects(const tArea &Area) const { return !(x2 < Area.x1 || x1 > Area.x2 || y2 < Area.y1 || y1 > Area.y2); }
};

#endif //__OSD_H
