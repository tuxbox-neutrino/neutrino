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
#define FB_ACCEL_CX2450X        0x90    /* Trident CX2450X             */

/*
 * structure which contains the image handle
 */
typedef struct _cnxtfb_handle
{
  void     *hImage;
  void     *hDRM_SD;
  void     *hTvEnc_SD;
  void     *hDRM;
  void     *hTvEnc;
} cnxtfb_handle;

typedef struct _cnxtfb_resolution
{
  u_int32_t uWidth;
  u_int32_t uHeight;

} cnxtfb_resolution;

typedef enum
{
  CNXTFB_BLEND_MODE_GLOBAL_ALPHA = 0, /* Global / Region Alpha */
  CNXTFB_BLEND_MODE_PIXEL_ALPHA,      /* Alpha from pixel */
  CNXTFB_BLEND_MODE_ALPHA_MULTIPLIED /* Global alpha multiplied with pixel alpha */
} CNXTFB_BLEND_MODE;

#define CNXTFB_IO(type)            _IO('F', type)
#define CNXTFB_IOW(type, dtype)    _IOW('F', type, dtype)
#define CNXTFB_IOR(type, dtype)    _IOR('F', type, dtype)

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC       _IOW('F', 0x20, u_int32_t)
#endif

#define FBIO_GETCNXTFBHANDLE      CNXTFB_IOR(0x21, cnxtfb_handle)
#define FBIO_STARTDISPLAY         CNXTFB_IO(0x22)
#define FBIO_STOPDISPLAY          CNXTFB_IO(0x23)
#define FBIO_SETOPACITY           CNXTFB_IOW(0x24, u_int8_t)
#define FBIO_SETBLENDMODE         CNXTFB_IOW(0x25, u_int8_t)
#define FBIO_CHANGEOUTPUTFORMAT   CNXTFB_IOW(0x26, u_int32_t)
#define FBIO_GETFBRESOLUTION      CNXTFB_IOR(0x27, cnxtfb_resolution)
#define FBIO_SETFLICKERFILTER     CNXTFB_IOW(0x28, u_int8_t)

#ifdef FB_TEST_HW_ACCELERATION
#define FBIO_FILL_RECT            CNXTFB_IOW(0x29, struct fb_fillrect)
#define FBIO_COPY_AREA            CNXTFB_IOW(0x2a, struct fb_copyarea)
#define FBIO_IMAGE_BLT            CNXTFB_IOW(0x2b, struct fb_image)
#endif
