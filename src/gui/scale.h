#ifndef __scale_
#define __scale_

#include <driver/framebuffer.h>

class CScale
{
 private:
    CFrameBuffer * frameBuffer;
    short width;
    short height;
    char percent;
    short red, green, yellow;
    bool inverse;
 public:
    CScale(int w, int h, int r, int g, int b, bool inv = false);
    void paint(int x, int y, int pcr);
    void hide();
    void reset();
    int getPercent() { return percent; };
};
#endif
