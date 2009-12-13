#ifndef __scale_
#define __scale_
/* define this if you do not want colorful blinkenlights
   the classical progressbars will be used instead. */
// #define NO_BLINKENLIGHTS
#include <driver/framebuffer.h>

#ifdef NO_BLINKENLIGHTS
#include <gui/widget/progressbar.h>
#endif

class CScale
{
 private:
#ifdef NO_BLINKENLIGHTS
    CProgressBar pb;
#else
    CFrameBuffer * frameBuffer;
    short red, green, yellow;
#endif
    short width;
    short height;
    char percent;
    bool inverse;

 public:
    CScale(int w, int h, int r, int g, int b, bool inv = false);
    void paint(int x, int y, int pcr);
    void hide();
    void reset();
    int getPercent() { return percent; };
};
#endif
