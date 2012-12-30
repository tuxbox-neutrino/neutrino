/*
 *  Driver for the Samsung HCR-13SS22 VF-Display connected to the Conexant
 *  CX2450x (Nevis) SoC via Coolstream VFD-Controller - public definititons.
 *	
 *  Copyright (C) 2008 Coolstream International Limited
 */

#ifndef __CS_VFD__
#define __CS_VFD__

typedef enum
{
    VFD_ICON_BAR8	= 0x00000004,
    VFD_ICON_BAR7	= 0x00000008,
    VFD_ICON_BAR6	= 0x00000010,
    VFD_ICON_BAR5	= 0x00000020,
    VFD_ICON_BAR4	= 0x00000040,
    VFD_ICON_BAR3	= 0x00000080,
    VFD_ICON_BAR2	= 0x00000100,
    VFD_ICON_BAR1	= 0x00000200,
    VFD_ICON_FRAME	= 0x00000400,
    VFD_ICON_HDD	= 0x00000800,
    VFD_ICON_MUTE	= 0x00001000,
    VFD_ICON_DOLBY	= 0x00002000,
    VFD_ICON_POWER	= 0x00004000,
    VFD_ICON_TIMESHIFT	= 0x00008000,
    VFD_ICON_SIGNAL	= 0x00010000,
    VFD_ICON_TV		= 0x00020000,
    VFD_ICON_RADIO	= 0x00040000,
    VFD_ICON_HD		= 0x01000001,
    VFD_ICON_1080P	= 0x02000001,
    VFD_ICON_1080I	= 0x03000001,
    VFD_ICON_720P	= 0x04000001,
    VFD_ICON_480P	= 0x05000001,
    VFD_ICON_480I	= 0x06000001,
    VFD_ICON_USB	= 0x07000001,
    VFD_ICON_MP3	= 0x08000001,
    VFD_ICON_PLAY	= 0x09000001,
    VFD_ICON_COL1	= 0x09000002,
    VFD_ICON_PAUSE	= 0x0A000001,
    VFD_ICON_CAM1	= 0x0B000001,
    VFD_ICON_COL2	= 0x0B000002,
    VFD_ICON_CAM2	= 0x0C000001
} vfd_icon;

typedef enum
{
    VFD_FLAG_NONE		= 0x00,
    VFD_FLAG_SCROLL_ON		= 0x01,	/* switch scrolling on */
    VFD_FLAG_SCROLL_LTR		= 0x02,	/* scroll from left to rightinstead of default right to left direction (i.e. for arabic text) */
    VFD_FLAG_SCROLL_SIO		= 0x04,	/* start/stop scrolling with empty screen (scroll in/out) */
    VFD_FLAG_SCROLL_DELAY	= 0x08,	/* delayed scroll start */
    VFD_FLAG_ALIGN_LEFT		= 0x10,	/* align the text in display from the left (default) */
    VFD_FLAG_ALIGN_RIGHT	= 0x20,	/* align the text in display from the right (arabic) */
    VFD_FLAG_UPDATE_SCROLL_POS	= 0x40,	/* update the current position for scrolling */
} vfd_flag;

typedef struct {
	unsigned char		brightness;
	unsigned char		flags;
	unsigned char		current_hour;
	unsigned char		current_minute;
	unsigned char		timer_minutes_hi;
	unsigned char		timer_minutes_lo;
} standby_data_t;

typedef enum {
	VFD_LED_1_ON		= 0x81,
	VFD_LED_2_ON		= 0x82,
	VFD_LED_3_ON		= 0x83,
	VFD_LED_1_OFF		= 0x01,
	VFD_LED_2_OFF		= 0x02,
	VFD_LED_3_OFF		= 0x03,
} vfd_led_ctrl_t;

typedef struct {
	unsigned char		source;
	unsigned char		time_minutes_hi;
	unsigned char		time_minutes_lo;
} wakeup_data_t;

typedef enum
{
	WAKEUP_SOURCE_TIMER	= 0x01,
	WAKEUP_SOURCE_BUTTON	= 0x02,
	WAKEUP_SOURCE_REMOTE	= 0x04,
	WAKEUP_SOURCE_PWLOST	= 0x7F,
	WAKEUP_SOURCE_POWER	= 0xFF
} wakeup_source;

#define IOC_VFD_SET_BRIGHT	_IOW(0xDE,  1, unsigned char)	/* set the display brighness in 16 steps between 0 to 15 */
#define IOC_VFD_CLEAR_ALL	_IOW(0xDE,  2, unsigned int)	/* clear the entire display (both text and icons) */
#define IOC_VFD_SET_TEXT	_IOW(0xDE,  3, char*)		/* set a text to be displayed on the display. If arg == NULL, the text is cleared */
#define IOC_VFD_SET_ICON	_IOW(0xDE,  4, vfd_icon)	/* switch the given icon on */
#define IOC_VFD_CLEAR_ICON	_IOW(0xDE,  5, vfd_icon)	/* switch the given icon off */
#define IOC_VFD_SET_OUTPUT	_IOW(0xDE,  6, unsigned char)	/* switch the given output on (supported by the controller, but not used in the hardware) */
#define IOC_VFD_CLEAR_OUTPUT	_IOW(0xDE,  7, unsigned char)	/* switch the given output off (supported by the controller, but not used in the hardware) */
#define IOC_VFD_STANDBY         _IOW(0xDE,  8, standby_data_t *)/* switch the vfd/psu in standby (NEO and above only) */
#define IOC_VFD_LED_CTRL        _IOW(0xDE,  9, unsigned char)   /* control the Frontpanles LED's (NEO and above only) */
#define IOC_VFD_GET_WAKEUP	_IOW(0xDE,  10,wakeup_data_t *) /* get wakeup data (NEO and above only) */

#endif /* __CS_VFD__ */
