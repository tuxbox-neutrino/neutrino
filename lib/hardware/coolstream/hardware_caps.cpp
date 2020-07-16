/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012,2016 Stefan Seyfried
 *
 * License: GPL v2 or later
 */
#include "cs_api.h"
#include <stdio.h>
#include <string.h>
#include "hardware_caps.h"

static int initialized = 0;
static hw_caps_t caps;

hw_caps_t *get_hwcaps(void) {
	if (initialized)
		return &caps;
	int rev = cs_get_revision();
	int chip = cs_get_chip_type();
	caps.has_fan = (rev < 8); // see dirty part of hw_caps in neutrino.cpp
	caps.has_HDMI = 1;
	caps.has_SCART = (rev != 10);
	caps.has_SCART_input = 0;
	caps.has_YUV_cinch = 1;
	caps.can_shutdown = (rev > 7);
	caps.can_cec = 1;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 12;
	caps.display_yres = 0;
	caps.display_can_deepstandby = (rev > 7);
	caps.display_can_set_brightness = 1;
	caps.display_has_statusline = 1;
	caps.display_has_colon = 0;
	caps.has_button_timer = 0;
	caps.has_button_vformat = 1;
	caps.can_ar_14_9 = 1;
	caps.can_ps_14_9 = 1;
	caps.force_tuner_2G = 0;
	strcpy(caps.boxvendor, "Coolstream");
	strcpy(caps.boxarch, "Nevis");
	switch (rev) {
	case 6:
	case 7: // Black Stallion Edition
		strcpy(caps.boxname, "HD1");
		strcpy(caps.boxarch, "Nevis");
		caps.force_tuner_2G = 1;
		break;
	case 8:
		strcpy(caps.boxname, "Neo"); // see dirty part of hw_caps in neutrino.cpp
		strcpy(caps.boxarch, "Nevis");
		caps.force_tuner_2G = 1;
		break;
	case 9:
		strcpy(caps.boxname, "Tank");
		strcpy(caps.boxarch, "Apollo");
		break;
	case 10:
		strcpy(caps.boxname, "Zee");
		caps.force_tuner_2G = 1;
		break;
	case 11:
		if (chip == CS_CHIP_SHINER)
		{
			strcpy(caps.boxname, "Trinity");
			strcpy(caps.boxarch, "Shiner");
		}
		else
		{
			strcpy(caps.boxname, "Trinity V2");
			strcpy(caps.boxarch, "Kronos");
		}
		caps.display_can_set_brightness = 0;
		caps.display_has_statusline = 0;
		break;
	case 12:
		strcpy(caps.boxname, "Zee2");
		strcpy(caps.boxarch, "Kronos");
		break;
	case 13:
		strcpy(caps.boxname, "Link");
		strcpy(caps.boxarch, "Kronos");
		caps.display_has_statusline = 0;
		break;
	case 14:
		strcpy(caps.boxname, "Trinity Duo");
		strcpy(caps.boxarch, "Kronos");
		caps.display_can_set_brightness = 0;
		caps.display_has_statusline = 0;
		break;
	default:
		strcpy(caps.boxname, "UNKNOWN_BOX");
		strcpy(caps.boxarch, "Unknown");
		fprintf(stderr, "[%s] unhandled box revision %d\n", __func__, rev);
	}
	initialized = 1;
	return &caps;
}
