/*******************************************************************************/
/*                                                                             */
/* libcoolstream/video_cs.h                                                    */
/*   Public header file for video API                                          */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/*******************************************************************************/
#ifndef __VIDEO_CS_H_
#define __VIDEO_CS_H_

#include <cs_vfd.h>
#include <control.h>
#include <dmx_cs.h>

#include "cs_types.h"

#ifndef CS_VIDEO_PDATA
#define CS_VIDEO_PDATA void
#endif

#define SCREENSHOT 1

typedef enum {
	ANALOG_SD_RGB_CINCH = 0x00,
	ANALOG_SD_YPRPB_CINCH,
	ANALOG_HD_RGB_CINCH,
	ANALOG_HD_YPRPB_CINCH,
	ANALOG_SD_RGB_SCART = 0x10,
	ANALOG_SD_YPRPB_SCART,
	ANALOG_HD_RGB_SCART,
	ANALOG_HD_YPRPB_SCART,
	ANALOG_SCART_MASK = 0x10
} analog_mode_t;

typedef enum {
	VIDEO_FORMAT_MPEG2 = 0,
	VIDEO_FORMAT_MPEG4,
	VIDEO_FORMAT_VC1,
	VIDEO_FORMAT_JPEG,
	VIDEO_FORMAT_GIF,
	VIDEO_FORMAT_PNG
} VIDEO_FORMAT;

typedef enum {
	VIDEO_SD = 0,
	VIDEO_HD,
	VIDEO_120x60i,
	VIDEO_320x240i,
	VIDEO_1440x800i,
	VIDEO_360x288i
} VIDEO_DEFINITION;

typedef enum {
	VIDEO_FRAME_RATE_23_976 = 0,
	VIDEO_FRAME_RATE_24,
	VIDEO_FRAME_RATE_25,
	VIDEO_FRAME_RATE_29_97,
	VIDEO_FRAME_RATE_30,
	VIDEO_FRAME_RATE_50,
	VIDEO_FRAME_RATE_59_94,
	VIDEO_FRAME_RATE_60
} VIDEO_FRAME_RATE;

typedef enum {
	DISPLAY_AR_1_1,
	DISPLAY_AR_4_3,
	DISPLAY_AR_14_9,
	DISPLAY_AR_16_9,
	DISPLAY_AR_20_9,
	DISPLAY_AR_RAW
} DISPLAY_AR;

typedef enum {
	DISPLAY_AR_MODE_PANSCAN = 0,
	DISPLAY_AR_MODE_LETTERBOX,
	DISPLAY_AR_MODE_NONE,
	DISPLAY_AR_MODE_PANSCAN2
} DISPLAY_AR_MODE;

typedef enum {
	VIDEO_DB_DR_NEITHER = 0,
	VIDEO_DB_ON,
	VIDEO_DB_DR_BOTH
} VIDEO_DB_DR;

typedef enum {
	VIDEO_PLAY_STILL = 0,
	VIDEO_PLAY_CLIP,
	VIDEO_PLAY_TRICK,
	VIDEO_PLAY_MOTION,
	VIDEO_PLAY_MOTION_NO_SYNC
} VIDEO_PLAY_MODE;

typedef enum {
	VIDEO_STD_NTSC,
	VIDEO_STD_SECAM,
	VIDEO_STD_PAL,
	VIDEO_STD_480P,
	VIDEO_STD_576P,
	VIDEO_STD_720P60,
	VIDEO_STD_1080I60,
	VIDEO_STD_720P50,
	VIDEO_STD_1080I50,
	VIDEO_STD_1080P30,
	VIDEO_STD_1080P24,
	VIDEO_STD_1080P25,
	VIDEO_STD_AUTO,
	VIDEO_STD_MAX = VIDEO_STD_AUTO
} VIDEO_STD;

typedef enum {
	VIDEO_HDMI_CEC_MODE_OFF	= 0,
	VIDEO_HDMI_CEC_MODE_TUNER,
	VIDEO_HDMI_CEC_MODE_RECORDER
} VIDEO_HDMI_CEC_MODE;

typedef enum
{
   VIDEO_CONTROL_BRIGHTNESS = 0,
   VIDEO_CONTROL_CONTRAST,
   VIDEO_CONTROL_SATURATION,
   VIDEO_CONTROL_HUE,
   VIDEO_CONTROL_SHARPNESS,
   VIDEO_CONTROL_MAX = VIDEO_CONTROL_SHARPNESS
} VIDEO_CONTROL;

class cVideo {
private:
	CS_VIDEO_PDATA		*privateData;
	VIDEO_FORMAT		StreamType;
	VIDEO_DEFINITION	VideoDefinition;
	DISPLAY_AR		DisplayAR;
	VIDEO_PLAY_MODE		playMode;
	AVSYNC_TYPE		syncMode;
	DISPLAY_AR_MODE		ARMode;
	VIDEO_DB_DR		eDbDr;
	DISPLAY_AR		PictureAR;
	VIDEO_FRAME_RATE	FrameRate;
	VIDEO_HDMI_CEC_MODE	hdmiCECMode;
	bool			Interlaced;
	unsigned int		uDRMDisplayDelay;
	unsigned int		uVideoPTSDelay;
	int			StcPts;
	bool			started;
	unsigned int		bStandby;
	bool			blank;
	bool			playing;
	bool			auto_format;
	int			uFormatIndex;
	bool			vbi_started;
	bool			receivedDRMDelay;
	bool			receivedVideoDelay;
	int			cfd; // control driver fd
	analog_mode_t		analog_mode_cinch;
	analog_mode_t		analog_mode_scart;
	vfd_icon		mode_icon;
	unsigned int		unit;
	cDemux			*demux;
	//
	int SelectAutoFormat();
	void ScalePic();
public:
	/* constructor & destructor */
	cVideo(int mode, void * hChannel, void * hBuffer, unsigned int Unit = 0);
	~cVideo(void);

	void * GetDRM(void);
	void * GetTVEnc();
	void * GetTVEncSD();
	void * GetHandle();

	void SetAudioHandle(void * handle);
	/* aspect ratio */
	int getAspectRatio(void);
	void getPictureInfo(int &width, int &height, int &rate);
	int setAspectRatio(int aspect, int mode);

	/* cropping mode */
	int getCroppingMode(void);
	int setCroppingMode(void);

	/* stream source */
	int getSource(void);
	int setSource(void);
	int GetStreamType(void) { return StreamType; };

	/* blank on freeze */
	int getBlank(void);
	int setBlank(int enable);

	/* get play state */
	int getPlayState(void);
	void SetDRMDelay(unsigned int delay) { uDRMDisplayDelay = delay;};
	void SetVideoDelay(unsigned int delay) { uVideoPTSDelay = delay;};
	/* Notification handlers */
	void HandleDRMMessage(int Event, void *pData);
	void HandleVideoMessage(void * hHandle, int Event, void *pData);
	void HandleEncoderMessage(void *hHandle, int Event, void *pData);
	VIDEO_DEFINITION   GetVideoDef(void) { return VideoDefinition; }

	/* change video play state */
	int Prepare(void * PcrChannel, unsigned short PcrPid, unsigned short VideoPid, void * hChannel = NULL);
	int Start(void * PcrChannel, unsigned short PcrPid, unsigned short VideoPid, void * hChannel = NULL);
	int Stop(bool Blank = true);
	bool Pause(void);
	bool Resume(void);
	int LipsyncAdjust();
	int64_t GetPTS(void);
	int Flush(void);

	/* set video_system */
	int SetVideoSystem(int video_system, bool remember = true);
	int SetStreamType(VIDEO_FORMAT type);
	void SetSyncMode(AVSYNC_TYPE mode);
	bool SetCECMode(VIDEO_HDMI_CEC_MODE Mode);
	void SetCECAutoView(bool OnOff);
	void SetCECAutoStandby(bool OnOff);
	void ShowPicture(const char * fname);
	void StopPicture();
	void Standby(bool bOn);
	void Pig(int x, int y, int w, int h, int osd_w = 1064, int osd_h = 600);
	void SetControl(int num, int val);
	bool ReceivedDRMDelay(void) { return receivedDRMDelay; }
	bool ReceivedVideoDelay(void) { return receivedVideoDelay; }
	void SetReceivedDRMDelay(bool Received) { receivedDRMDelay = Received; }
	void SetReceivedVideoDelay(bool Received) { receivedVideoDelay = Received; }
	void SetFastBlank(bool onoff);
	void SetTVAV(bool onoff);
	void SetWideScreen(bool onoff);
	void SetVideoMode(analog_mode_t mode);
	void SetDBDR(int dbdr);
	void SetAutoModes(int modes[VIDEO_STD_MAX]);
	int  OpenVBI(int num);
	int  CloseVBI(void);
	int  StartVBI(unsigned short pid);
	int  StopVBI(void);
	bool GetScreenImage(unsigned char * &data, int &xres, int &yres, bool get_video = true, bool get_osd = false, bool scale_to_video = false);
	void SetDemux(cDemux *Demux);
};

#endif // __VIDEO_CS_H_
