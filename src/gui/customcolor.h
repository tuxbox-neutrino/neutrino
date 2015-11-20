#ifndef __custom_color__
#define __custom_color__

#define COLOR_CUSTOM 0x0
#ifdef FB_USE_PALETTE
/*
#define COL_WHITE (COLOR_CUSTOM + 0)
#define COL_RED (COLOR_CUSTOM + 1)
#define COL_GREEN (COLOR_CUSTOM + 2)
#define COL_BLUE (COLOR_CUSTOM + 3)
#define COL_YELLOW (COLOR_CUSTOM + 4)
#define COL_BLACK (COLOR_CUSTOM + 5)
*/
#define COL_DARK_RED 0x02
#define COL_DARK_GREEN 0x03
#define COL_OLIVE 0x04
#define COL_DARK_BLUE 0x05
#define COL_LIGHT_GRAY 0x08
#define COL_DARK_GRAY 0x09
#define COL_RED 0x0A
#define COL_GREEN 0x0B
#define COL_YELLOW 0x0C
#define COL_BLUE 0x0D
#define COL_PURP 0x0E
#define COL_LIGHT_BLUE 0x0F
#define COL_WHITE 0x10
#define COL_BLACK 0x11
#else
#define COL_DARK_RED0	 0x02
#define COL_DARK_GREEN0	 0x03
#define COL_OLIVE0 	 0x04
#define COL_DARK_BLUE0	 0x05
#define COL_LIGHT_GRAY0	 0x08
#define COL_DARK_GRAY0	 0x09
#define COL_RED0	 0x0A
#define COL_GREEN0	 0x0B
#define COL_YELLOW0	 0x0C
#define COL_BLUE0	 0x0D
#define COL_PURP0	 0x0E
#define COL_LIGHT_BLUE0	 0x0F
#define COL_WHITE0	 0x10
#define COL_BLACK0	 0x11

#define COL_DARK_RED			(CFrameBuffer::getInstance()->realcolor[0x02])
#define COL_DARK_GREEN			(CFrameBuffer::getInstance()->realcolor[0x03])
#define COL_OLIVE 			(CFrameBuffer::getInstance()->realcolor[0x04])
#define COL_DARK_BLUE			(CFrameBuffer::getInstance()->realcolor[0x05])
#define COL_LIGHT_GRAY			(CFrameBuffer::getInstance()->realcolor[0x08])
#define COL_DARK_GRAY			(CFrameBuffer::getInstance()->realcolor[0x09])
#define COL_RED				(CFrameBuffer::getInstance()->realcolor[0x0A])
#define COL_GREEN			(CFrameBuffer::getInstance()->realcolor[0x0B])
#define COL_YELLOW			(CFrameBuffer::getInstance()->realcolor[0x0C])
#define COL_BLUE                        (CFrameBuffer::getInstance()->realcolor[0x0D])
#define COL_PURP 			(CFrameBuffer::getInstance()->realcolor[0x0E])
#define COL_LIGHT_BLUE 			(CFrameBuffer::getInstance()->realcolor[0x0F])
#define COL_WHITE                       (CFrameBuffer::getInstance()->realcolor[0x10])
#define COL_BLACK                       (CFrameBuffer::getInstance()->realcolor[0x11])
#endif

#endif
