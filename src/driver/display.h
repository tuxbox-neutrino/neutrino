/* helper for different display CVFD implementations */
#if HAVE_CST_HARDWARE
#include <driver/vfd.h>
#endif
#if HAVE_GENERIC_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#if BOXMODEL_OSMIO4KPLUS
#include <driver/lcdd.h>
#define CVFD CLCD
#else
#include <driver/simple_display.h>
#endif
#endif
#ifdef ENABLE_GRAPHLCD
#include <driver/glcd/glcd.h>
#endif


#ifndef __DISPLAY_H__
#define __DISPLAY_H__

class CDisplay
{
	public:
		CDisplay(){};
		virtual ~CDisplay(){};

		virtual void showServicename(const std::string, const bool ){}; // UTF-8
		virtual void showServicename(const std::string, int){}; // UTF-8
};

#endif
