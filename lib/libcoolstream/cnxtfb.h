#ifndef __CNXTFB_H__
#define __CNXTFB_H__
/****************************************************************************/
/* $Id: 
 ****************************************************************************/
#include <linux/fb.h>
#include <linux/ioctl.h>
#include <linux/types.h>
/*
 * define the IOCTL to get the frame buffer handle info.
 * currently only image handle is returned
 */

/* define this for testing H/w acceleation funcitons */
/* #define FB_TEST_HW_ACCELERATION */


/* assign an accelerator type (we have no official, so we do not add them to linux/fb.h */
#define FB_ACCEL_PNX849X        0x90    /* Trident PNX849X             */

struct fb_info;

/*
 * structure which contains the image handle
 */
typedef struct _cnxtfb_handles
{
  void     *hImage;
  void     *hVPP_SD;
  void     *hTvEnc_SD;
  void     *hVPP;
  void     *hTvEnc;
  void     *hImage_SD;
} cnxtfb_handles;

typedef struct _cnxtfb_resolution
{
  u_int32_t uWidth;
  u_int32_t uHeight;

} cnxtfb_resolution;

/* To use with ioctl FBIO_CHANGEOUTPUTFORMAT */
typedef enum {
    CNXTFB_VIDEO_STANDARD_ATSC_1080I = 0,
    CNXTFB_VIDEO_STANDARD_NTSC_M,
    CNXTFB_VIDEO_STANDARD_ATSC_480P,
    CNXTFB_VIDEO_STANDARD_ATSC_720P,
    CNXTFB_VIDEO_STANDARD_PAL_B_WEUR,
    CNXTFB_VIDEO_STANDARD_SECAM_L,
    CNXTFB_VIDEO_STANDARD_ATSC_576P,
    CNXTFB_VIDEO_STANDARD_ATSC_720P_50HZ,
    CNXTFB_VIDEO_STANDARD_ATSC_1080I_50HZ
} CNXTFB_VIDEO_STANDARD;

typedef enum
{
   CNXTFB_BLEND_MODE_PER_PIXEL = 0,
   CNXTFB_BLEND_MODE_UNIFORM_ALPHA,
   /* Reordered for compatability .. */
   CNXTFB_BLEND_MODE_ALPHA_MULTIPLIED,
} CNXTFB_BLEND_MODE;

typedef enum
{
    CNXTFB_INVALID = -1,
    CNXTFB_480I = 0,
    CNXTFB_480P,
    CNXTFB_576I,
    CNXTFB_576P,
    CNXTFB_720P,
    CNXTFB_720P_50,
    CNXTFB_1080I,
    CNXTFB_1080I_50,
    CNXTFB_1080P,
    CNXTFB_1080P_50,
    CNXTFB_1080P_24,
    CNXTFB_1080P_25,
    CNXTFB_DISPLAY_MODE_LAST = CNXTFB_1080P_25,
} cnxtfb_displaymode;

typedef enum
{
   CNXTFB_TYPE_SD = 1, /* 1 << 0 */
   CNXTFB_TYPE_HD = 2  /* 1 << 1 */
} CNXTFB_FB_TYPE;

typedef struct
{
   unsigned char Type; /* Bitmask of type CNXTFB_FB_TYPE */
   cnxtfb_displaymode SDMode;
   cnxtfb_displaymode HDMode;
} CNXTFB_OUTPUT_FORMAT_CHANGE;

/*
 * structure which contains the image handle
 */
typedef struct _cnxtfb_handle
{
  /* CNXT_IMAGE_HANDLE         hImage; */
  void                        *hImage;
} cnxtfb_handle;

typedef enum
{
   CNXTFB_VSYNC_NOTIFICATION = 0x1000,
   CNXTFB_BUF_REELASE_NOTIFICATION,
   CNXTFB_DISPLAY_MODE_NOTIFICATION
}cnxtfb_event;

typedef struct
{
   u8  uRed;
   u8  uGreen;
   u8  uBlue;
   u8  uAlpha;
} CNXTFB_RGB_COLOR;

typedef struct
{
   u8  uY;
   u8  uCb;
   u8  uCr;
   u8  uAlpha;
} CNXTFB_YCC_COLOR;

typedef enum
{
   CNXTFB_COLOR_RGB = 0,         /* RGB format */
   CNXTFB_COLOR_YCC,             /* YCC format */
   CNXTFB_COLOR_PAL_INDEX,       /* Palette index or u_int32 representation of color*/
   CNXTFB_COLOR_TYPE_LAST = CNXTFB_COLOR_PAL_INDEX
} CNXTFB_COLOR_TYPE;

typedef union
{
   u_int32_t         uValue; /* Palette index or u_int32 representation of color */
   CNXTFB_RGB_COLOR  RGB;
   CNXTFB_YCC_COLOR  YCC;
} CNXTFB_COLOR_ENTRY;

typedef enum
{
   CNXTFB_YCC_BASIC = 0,   /* Pure YCbCr for MPEG-1 decodes */
   CNXTFB_YCC_SD_BT470,    /* ITU-R BT470-2 System M   */
   CNXTFB_YCC_SD_BT470_BG, /* ITU-R BT470-2 System B/G */
   CNXTFB_YCC_SMPTE_170M,  /* SMPTE 170M */
   CNXTFB_YCC_SMPTE_240M,  /* SMPTE 240M */
   CNXTFB_YCC_GEN_FILM,    /* Generic Film(Color filters using Illuminant C) */
   CNXTFB_YCC_HD_BT709,
   CNXTFB_RGB,
   CNXTFB_COLOR_SPACE_LAST = CNXTFB_RGB
} CNXTFB_COLOR_SPACE;

typedef struct
{
   CNXTFB_COLOR_SPACE     ColorSpace;
   CNXTFB_COLOR_ENTRY     Color;
} CNXTFB_COLOR_SPEC;


/* Enumeration for types of chroma key configurations. */
typedef enum
{
   CNXTFB_REGION_CHROMAKEY_SRC,
   CNXTFB_REGION_CHROMAKEY_DST
} CNXTFB_REGION_CHROMAKEY_TYPE;

typedef struct
{
   CNXTFB_COLOR_SPEC ColorKeyLower;
   CNXTFB_COLOR_SPEC ColorKeyUpper;
} CNXTFB_REGION_CHROMAKEY_CFG;

typedef struct _cnxtfb_chromakey_cfg{
   bool                          bEnable;
   CNXTFB_REGION_CHROMAKEY_TYPE  Type;
   CNXTFB_REGION_CHROMAKEY_CFG   *pCfg;
} cnxtfb_chromakey_cfg;

typedef void (*cnxtfb_notify)(cnxtfb_event event, void *cookie);

extern void cnxtfb_register_client(struct fb_info *fb_info, cnxtfb_notify pfnotify, void *cookie);
extern void cnxtfb_get_image_handle(struct fb_info *fb_info, void  **phImage);
extern void cnxtfb_get_handles(struct fb_info *fb_info, cnxtfb_handles *phandles);
extern void cnxtfb_register_evnt_clbk(cnxtfb_notify pfnotify);

#define CNXTFB_IO(type)            _IO('F', type)
#define CNXTFB_IOW(type, dtype)    _IOW('F', type, dtype)
#define CNXTFB_IOR(type, dtype)    _IOR('F', type, dtype)


#define FB_TEST_HW_ACCELERATION

#define FBIOGET_CNXTFBHANDLE      0x4620
#define FBIO_WAITFORVSYNC         0x4621
#define FBIO_STARTDISPLAY         0x4622
#define FBIO_STOPDISPLAY          0x4623
#define FBIO_SETBLENDMODE         0x4624
#define FBIO_CHANGEOUTPUTFORMAT   0x4625
#define FBIO_GETFBRESOLUTION      0x4626

#ifdef FB_TEST_HW_ACCELERATION
#define FBIO_FILL_RECT            0x4627
#define FBIO_COPY_AREA            0x4628
#define FBIO_IMAGE_BLT            0x4629
#define FBIO_STRETCH_COPY         0x4630
#endif

#define FBIO_SETOPACITY           0x4631
#define FBIO_FLIPBUFFER           0x4632

#ifdef FB_TEST_HW_ACCELERATION
#define FBIO_JPEG_RENDER          0x4633
#endif

#define FBIO_SCALE_SD_OSD         0x4634
#define FBIO_CHROMAKEY_CFG        0x4635
#define FBIO_DELAY_BUF_RELEASE    0x4636
/* CST Mod */
#define FBIO_GETCNXTFBHANDLES     0x4640


#if 0
#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC       _IOW('F', 0x20, u_int32_t)
#endif

#define FBIO_GETCNXTFBHANDLE      CNXTFB_IOR(0x21, cnxtfb_handle) // 0x4620
#define FBIO_STARTDISPLAY         CNXTFB_IO(0x22)
#define FBIO_STOPDISPLAY          CNXTFB_IO(0x23)
#define FBIO_SETOPACITY           CNXTFB_IOW(0x24, u_int8_t)
#define FBIO_SETBLENDMODE         CNXTFB_IOW(0x25, u_int8_t)
#define FBIO_CHANGEOUTPUTFORMAT   CNXTFB_IOW(0x26, u_int32_t)
#define FBIO_GETFBRESOLUTION      CNXTFB_IOR(0x27, cnxtfb_resolution)
#define FBIO_SETFLICKERFILTER     CNXTFB_IOW(0x28, u_int8_t)
#define FBIO_SCALE_SD_OSD         CNXTFB_IO(0x28)

#ifdef FB_TEST_HW_ACCELERATION
#define FBIO_FILL_RECT            CNXTFB_IOW(0x29, struct fb_fillrect)
#define FBIO_COPY_AREA            CNXTFB_IOW(0x2a, struct fb_copyarea)
#define FBIO_IMAGE_BLT            CNXTFB_IOW(0x2b, struct fb_image)
#endif
#endif

#endif /*  __CNXTFB_H__ */
