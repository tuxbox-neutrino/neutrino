#ifndef __color_custom__
#define __color_custom__

#define COLOR_CUSTOM	0x0

#ifdef FB_USE_PALETTE

#define COL_WHITE		0x02
#define COL_BLACK		0x03
#define COL_DARK_RED		0x04
#define COL_RED			0x05
#define COL_LIGHT_RED		0x06
#define COL_DARK_GREEN		0x07
#define COL_GREEN		0x08
#define COL_LIGHT_GREEN		0x09
#define COL_DARK_YELLOW		0x0A
#define COL_YELLOW		0x0B
#define COL_LIGHT_YELLOW	0x0C
#define COL_DARK_BLUE		0x0D
#define COL_BLUE		0x0E
#define COL_LIGHT_BLUE		0x0F
#define COL_DARK_GRAY		0x10
#define COL_GRAY		0x11
#define COL_LIGHT_GRAY		0x12

#else

#define COL_WHITE0		0x02
#define COL_BLACK0		0x03
#define COL_DARK_RED0		0x04
#define COL_RED0		0x05
#define COL_LIGHT_RED0		0x06
#define COL_DARK_GREEN0		0x07
#define COL_GREEN0		0x08
#define COL_LIGHT_GREEN0	0x09
#define COL_DARK_YELLOW0	0x0A
#define COL_YELLOW0		0x0B
#define COL_LIGHT_YELLOW0	0x0C
#define COL_DARK_BLUE0		0x0D
#define COL_BLUE0		0x0E
#define COL_LIGHT_BLUE0		0x0F
#define COL_DARK_GRAY0		0x10
#define COL_GRAY0		0x11
#define COL_LIGHT_GRAY0		0x12

#define COL_WHITE		(CFrameBuffer::getInstance()->realcolor[0x02])
#define COL_BLACK		(CFrameBuffer::getInstance()->realcolor[0x03])
#define COL_DARK_RED		(CFrameBuffer::getInstance()->realcolor[0x04])
#define COL_RED			(CFrameBuffer::getInstance()->realcolor[0x05])
#define COL_LIGHT_RED		(CFrameBuffer::getInstance()->realcolor[0x06])
#define COL_DARK_GREEN		(CFrameBuffer::getInstance()->realcolor[0x07])
#define COL_GREEN		(CFrameBuffer::getInstance()->realcolor[0x08])
#define COL_LIGHT_GREEN		(CFrameBuffer::getInstance()->realcolor[0x09])
#define COL_DARK_YELLOW		(CFrameBuffer::getInstance()->realcolor[0x0A])
#define COL_YELLOW		(CFrameBuffer::getInstance()->realcolor[0x0B])
#define COL_LIGHT_YELLOW	(CFrameBuffer::getInstance()->realcolor[0x0C])
#define COL_DARK_BLUE		(CFrameBuffer::getInstance()->realcolor[0x0D])
#define COL_BLUE		(CFrameBuffer::getInstance()->realcolor[0x0E])
#define COL_LIGHT_BLUE		(CFrameBuffer::getInstance()->realcolor[0x0F])
#define COL_DARK_GRAY		(CFrameBuffer::getInstance()->realcolor[0x10])
#define COL_GRAY		(CFrameBuffer::getInstance()->realcolor[0x11])
#define COL_LIGHT_GRAY		(CFrameBuffer::getInstance()->realcolor[0x12])

#endif // FB_USE_PALETTE

#define COL_RANDOM	(getRandomColor())

#endif
