/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012 Stefan Seyfried
 *
 * License: GPL v2 or later
 */
#include "hardware_caps.h"
static int initialized = 0;
static hw_caps_t caps;

hw_caps_t *get_hwcaps(void) {
	if (initialized)
		return &caps;
	caps.has_fan = (cs_get_revision() < 8);
	caps.has_HDMI = 1;
	caps.has_SCART = (cs_get_revision() != 10);
	caps.has_SCART_input = 0;
	caps.has_YUV_cinch = 1;
	caps.can_shutdown = (cs_get_revision() > 7);
	caps.can_cec = 1;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 12;
	caps.display_yres = 0;
	caps.can_set_display_brightness = 1;
	strcpy(caps.boxvendor, "Coolstream");
	if (cs_get_revision() < 8)
		strcpy(caps.boxname, "HD1");
	else if (cs_get_revision() == 10)
		strcpy(caps.boxname, "ZEE");
	else
		strcpy(caps.boxname, "NEO");
	initialized = 1;
	return &caps;
}

