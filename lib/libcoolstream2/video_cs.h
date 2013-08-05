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

#include <coolstream/cs_frontpanel.h>
#include <coolstream/control.h>

#include "cs_types.h"

#define CS_MAX_VIDEO_DECODERS 16

#ifndef CS_VIDEO_PDATA
#define CS_VIDEO_PDATA void
#endif

typedef enum {
	// Video modes
	ANALOG_xD_CVBS		= (1 << 0), // Turns off fastblank on SCART
	ANALOG_SD_RGB		= (1 << 1), // Output SD in RGB format
	ANALOG_SD_YPRPB		= (1 << 2), // Output SD in YPbPr format (component)
	ANALOG_HD_RGB		= (1 << 3), // Output HD in RGB format
	ANALOG_HD_YPRPB		= (1 << 4), // Output HD in YPbPr format (component)
	ANALOG_xD_AUTO		= (1 << 5), // Output is automatically determined based on
					    // content. (TANK/Trinity only)
	// Output types
	ANALOG_xD_SCART		= (1 << 8), // Output is SCART
	ANALOG_xD_CINCH		= (1 << 9), // Output is Cinch
	ANALOG_xD_BOTH		= (3 << 8), // Output cannot individually control scart, cinch outputs
					    // due to limited amount of DACs (ie TANK, Trinity)
} analog_mode_t;

#define ANALOG_MODE(conn, def, sys)	(ANALOG_##def##_##sys | ANALOG_xD_##conn)
#define ANALOG_MODE_VIDEO(mode)		(mode & ((1 << 6) - 1))
#define ANALOG_MODE_OUTPUT(mode)	(mode & ANALOG_xD_BOTH)
#define ANALOG_MODE_HD(mode)		(mode & (ANALOG_HD_RGB | ANALOG_HD_YPRPB))

typedef enum
{
	VIDEO_FORMAT_MPEG2 = 0,
	VIDEO_FORMAT_MPEG4, /* H264 */
	VIDEO_FORMAT_VC1,
	VIDEO_FORMAT_JPEG,
	VIDEO_FORMAT_GIF,
	VIDEO_FORMAT_PNG,
	VIDEO_FORMAT_DIVX,/* DIVX 3.11 */
	VIDEO_FORMAT_MPEG4PART2,/* MPEG4 SVH, MPEG4 SP, MPEG4 ASP, DIVX4,5,6 */
	VIDEO_FORMAT_REALVIDEO8,
	VIDEO_FORMAT_REALVIDEO9,
	VIDEO_FORMAT_ON2_VP6,
	VIDEO_FORMAT_ON2_VP8,
	VIDEO_FORMAT_SORENSON_SPARK,
	VIDEO_FORMAT_H263,
	VIDEO_FORMAT_H263_ENCODER,
	VIDEO_FORMAT_H264_ENCODER,
	VIDEO_FORMAT_MPEG4PART2_ENCODER,
	VIDEO_FORMAT_AVS,
	VIDEO_FORMAT_VIP656,
	VIDEO_FORMAT_UNSUPPORTED
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
	VIDEO_STD_1080P50,
	VIDEO_STD_1080P60,
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

class cDemux;
class cAudio;

class cVideo {
friend class cAudio;
private:
	static cVideo *instance[CS_MAX_VIDEO_DECODERS];

	unsigned int		unit;
	CS_VIDEO_PDATA		*privateData;
	VIDEO_FORMAT		streamType;
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
	unsigned int		uVPPDisplayDelay;
	unsigned int		uVideoPTSDelay;
	int			StcPts;
	bool			started;
	unsigned int		bStandby;
	bool			blank;
	bool			playing;
	bool			auto_format;
	int			uFormatIndex;
	bool			vbi_started;
	bool			receivedVPPDelay;
	bool			receivedVideoDelay;
	int			cfd; // control driver fd
	analog_mode_t		analog_mode_cinch;
	analog_mode_t		analog_mode_scart;
	fp_icon			mode_icon;
	cDemux			*demux;
	//
	int SelectAutoFormat();
	void ScalePic();
	cVideo(unsigned int Unit);
public:
	/* constructor & destructor */
	cVideo(int mode, void * hChannel, void * hBuffer);
	~cVideo(void);

	void * GetVPP(void);
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
	int GetStreamType(void);

	/* blank on freeze */
	int getBlank(void);
	int setBlank(int enable);

	/* get play state */
	int getPlayState(void);
	void SetVPPDelay(unsigned int delay) { uVPPDisplayDelay = delay;};
	void SetVideoDelay(unsigned int delay) { uVideoPTSDelay = delay;};
	/* Notification handlers */
	void HandleVPPMessage(int Event, void *pData);
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
	bool ReceivedVPPDelay(void) { return receivedVPPDelay; }
	bool ReceivedVideoDelay(void) { return receivedVideoDelay; }
	void SetReceivedVPPDelay(bool Received) { receivedVPPDelay = Received; }
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
	static cVideo *GetDecoder(unsigned int Unit);
};

#endif // __VIDEO_CS_H_
