#ifndef _VIDEO_TD_H
#define _VIDEO_TD_H

#include <hardware/vid/vid_inf.h>
#define video_format_t          vidDispSize_t
//#define video_displayformat_t   vidDispMode_t


typedef enum {
	ANALOG_SD_RGB_SCART = 0x00,
	ANALOG_SD_YPRPB_SCART,
	ANALOG_HD_RGB_SCART,
	ANALOG_HD_YPRPB_SCART,
	ANALOG_SD_RGB_CINCH = 0x80,
	ANALOG_SD_YPRPB_CINCH,
	ANALOG_HD_RGB_CINCH,
	ANALOG_HD_YPRPB_CINCH,
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
	DISPLAY_AR_RAW,
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
	VIDEO_STD_NTSC	= VID_DISPFMT_NTSC,	/* 0 */
	VIDEO_STD_PAL	= VID_DISPFMT_PAL,	/* 1 */
	VIDEO_STD_SECAM	= VID_DISPFMT_SECAM,	/* 4 */
	VIDEO_STD_1080I50 = VIDEO_STD_PAL,	/* hack, this is used in neutrino settings default */
	VIDEO_STD_MAX = VIDEO_STD_SECAM
} VIDEO_STD;

typedef enum {
	VIDEO_STOPPED, /* Video is stopped */
	VIDEO_PLAYING, /* Video is currently playing */
	VIDEO_FREEZED  /* Video is freezed */
} video_play_state_t;

/* not used, for dummy functions */
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


class cVideo
{
	private:
		/* video device */
		int fd;
		/* apparently we cannot query the driver's state
		   => remember it */
		video_play_state_t playstate;
		vidDispMode_t croppingMode;
		vidOutFmt_t outputformat;
		int scartvoltage;
		int z[2]; /* zoomvalue for 4:3 (0) and 16:9 (1) in percent */
		int *zoomvalue;
		void *blank_data[2]; /* we store two blank MPEGs (PAL/NTSC) in there */
		int blank_size[2];

		VIDEO_FORMAT	    StreamType;
		VIDEO_DEFINITION       VideoDefinition;
		DISPLAY_AR DisplayAR;
		VIDEO_PLAY_MODE SyncMode;
		DISPLAY_AR_MODE                ARMode;
		VIDEO_DB_DR eDbDr;
		DISPLAY_AR PictureAR;
		VIDEO_FRAME_RATE FrameRate;
		void routeVideo(int standby);
		int video_standby;
	public:
		/* constructor & destructor */
		cVideo(int mode, void *, void *);
		~cVideo(void);

		void * GetTVEnc() { return NULL; };
		void * GetTVEncSD() { return NULL; };

		/* aspect ratio */
		int getAspectRatio(void);
		void getPictureInfo(int &width, int &height, int &rate);
		int setAspectRatio(int aspect, int mode);

		/* cropping mode */
		int setCroppingMode(vidDispMode_t x = VID_DISPMODE_NORM);

		/* get play state */
		int getPlayState(void);

		/* blank on freeze */
		int getBlank(void);
		int setBlank(int enable);

		/* change video play state. Parameters are all unused. */
		int Start(void *PcrChannel = NULL, unsigned short PcrPid = 0, unsigned short VideoPid = 0, void *x = NULL);
		int Stop(bool blank = true);
		bool Pause(void);

		/* set video_system */
		int SetVideoSystem(int video_system, bool remember = true);
		int SetStreamType(VIDEO_FORMAT type);
#define AVSYNC_TYPE int
		void SetSyncMode(AVSYNC_TYPE mode);
		bool SetCECMode(VIDEO_HDMI_CEC_MODE) { return true; };
		void SetCECAutoView(bool) { return; };
		void SetCECAutoStandby(bool) { return; };
		void ShowPicture(const char * fname);
		void StopPicture();
		void Standby(unsigned int bOn);
		void Pig(int x, int y, int w, int h, int osd_w = 1064, int osd_h = 600);
		void SetControl(int, int) { return; };
		int setZoom(int);
		void VideoParamWatchdog(void);
		void setContrast(int val);
		void SetVideoMode(analog_mode_t mode);
		void SetDBDR(int) { return; };
		void SetAudioHandle(void *) { return; };
		void FastForwardMode(int mode = 0);
		void SetAutoModes(int [VIDEO_STD_MAX]) { return; };
		int  OpenVBI(int) { return 0; };
		int  CloseVBI(void) { return 0; };
		int  StartVBI(unsigned short) { return 0; };
		int  StopVBI(void) { return 0; };
};

#endif
