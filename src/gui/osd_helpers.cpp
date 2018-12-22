
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include <driver/framebuffer.h>

#include <gui/infoclock.h>
#include <gui/infoviewer.h>
#include <gui/timeosd.h>
#include <gui/volumebar.h>
#include <gui/osd_helpers.h>

#include <hardware/video.h>

extern CInfoClock *InfoClock;
extern CTimeOSD *FileTimeOSD;
extern cVideo *videoDecoder;

COsdHelpers::COsdHelpers()
{
	g_settings_osd_resolution_save	= 0;
}

COsdHelpers::~COsdHelpers()
{
}

COsdHelpers* COsdHelpers::getInstance()
{
	static COsdHelpers* osdh = NULL;
	if(!osdh)
		osdh = new COsdHelpers();

	return osdh;
}

#ifdef ENABLE_CHANGE_OSD_RESOLUTION
void COsdHelpers::changeOsdResolution(uint32_t mode, bool automode/*=false*/, bool forceOsdReset/*=false*/)
{
	size_t idx = 0;
	bool resetOsd = false;
	uint32_t modeNew;

	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();

	if (automode) {
		if (g_settings.video_Mode == VIDEO_STD_AUTO)
			modeNew = OSDMODE_1080;
		else
			modeNew = g_settings_osd_resolution_save;
	}
	else {
		modeNew = mode;
	}

	int videoSystem = getVideoSystem();

	if ((g_settings.video_Mode == VIDEO_STD_AUTO) &&
	    (g_settings.enabled_auto_modes[videoSystem] == 1) &&
	    (!isVideoSystem1080(videoSystem)))
		modeNew = OSDMODE_720;

//	if (!isVideoSystem1080(videoSystem))
//		modeNew = OSDMODE_720;

	idx = frameBuffer->getIndexOsdResolution(modeNew);
	resetOsd = (modeNew != getOsdResolution()) ? true : false;
#if 1
	printf(">>>>>[%s:%d] osd mode: %s => %s, automode: %s, forceOsdReset: %s\n", __func__, __LINE__,
		(g_settings.osd_resolution == OSDMODE_720)?"OSDMODE_720":"OSDMODE_1080",
		(modeNew == OSDMODE_720)?"OSDMODE_720":"OSDMODE_1080",
		(automode)?"true":"false",
		(forceOsdReset)?"true":"false");
#endif
	if (forceOsdReset)
		resetOsd = true;

	if (frameBuffer->fullHdAvailable()) {
		if (frameBuffer->osd_resolutions.empty())
			return;

		bool ivVisible = false;
		if (g_InfoViewer && g_InfoViewer->is_visible) {
			g_InfoViewer->killTitle();
			ivVisible = true;
		}

		int switchFB = frameBuffer->setMode(frameBuffer->osd_resolutions[idx].xRes,
						    frameBuffer->osd_resolutions[idx].yRes,
						    frameBuffer->osd_resolutions[idx].bpp);

		if (switchFB == 0) {
//printf("\n>>>>>[%s:%d] New res: %dx%dx%d\n \n", __func__, __LINE__, resW, resH, bpp);
			g_settings.osd_resolution = modeNew;
			if (InfoClock)
				InfoClock->enableInfoClock(false);
			frameBuffer->Clear();
			if (resetOsd) {
				CNeutrinoApp::getInstance()->setScreenSettings();
				CNeutrinoApp::getInstance()->SetupFonts(CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT);
				CVolumeHelper::getInstance()->refresh();
				if (InfoClock)
					CInfoClock::getInstance()->ClearDisplay();
				if (FileTimeOSD)
					FileTimeOSD->Init();
				if (CNeutrinoApp::getInstance()->channelList)
					CNeutrinoApp::getInstance()->channelList->ResetModules();
			}
			if (InfoClock)
				InfoClock->enableInfoClock(true);
		}
		if (g_InfoViewer) {
			g_InfoViewer->ResetModules();
			g_InfoViewer->start();
		}
		if (ivVisible) {
			CNeutrinoApp::getInstance()->StopSubtitles();
			g_InfoViewer->showTitle(CNeutrinoApp::getInstance()->channelList->getActiveChannel(), true, 0, true);
			CNeutrinoApp::getInstance()->StartSubtitles();
		}
	}
}
#else
void COsdHelpers::changeOsdResolution(uint32_t, bool, bool)
{
}
#endif

int COsdHelpers::isVideoSystem1080(int res)
{
	if ((res == VIDEO_STD_1080I60) ||
	    (res == VIDEO_STD_1080I50) ||
	    (res == VIDEO_STD_1080P30) ||
	    (res == VIDEO_STD_1080P24) ||
	    (res == VIDEO_STD_1080P25))
		return true;

#ifdef BOXMODEL_CS_HD2
	if ((res == VIDEO_STD_1080P50) ||
	    (res == VIDEO_STD_1080P60) ||
	    (res == VIDEO_STD_1080P2397) ||
	    (res == VIDEO_STD_1080P2997))
		return true;
#endif

#if HAVE_ARM_HARDWARE
	if ((res == VIDEO_STD_1080P50) ||
	    (res == VIDEO_STD_1080P60) ||
	    (res == VIDEO_STD_2160P24) ||
	    (res == VIDEO_STD_2160P25) ||
	    (res == VIDEO_STD_2160P30) ||
	    (res == VIDEO_STD_2160P50))
		return true;
#endif

#if 0
	/* for testing only */
	if (res == VIDEO_STD_720P50)
		return true;
#endif

	return false;
}

int COsdHelpers::getVideoSystem()
{
	return videoDecoder->GetVideoSystem();
}

uint32_t COsdHelpers::getOsdResolution()
{
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	if (frameBuffer->osd_resolutions.size() == 1)
		return 0;

	uint32_t yRes = frameBuffer->getScreenHeight(true);
	for (size_t i = 0; i < frameBuffer->osd_resolutions.size(); i++) {
		if (frameBuffer->osd_resolutions[i].yRes == yRes)
			return frameBuffer->osd_resolutions[i].mode;
	}
	return 0;
}

#define DEBUGINFO_SETVIDEOSYSTEM

int COsdHelpers::setVideoSystem(int newSystem, bool remember/* = true*/)
{
	if ((newSystem < 0) || (newSystem > VIDEO_STD_MAX))
		return -1;

	if (newSystem == getVideoSystem())
		return 0;

#ifdef DEBUGINFO_SETVIDEOSYSTEM
	int fd = CFrameBuffer::getInstance()->getFileHandle();
	fb_var_screeninfo var;
	fb_fix_screeninfo fix;

	ioctl(fd, FBIOGET_VSCREENINFO, &var);
	ioctl(fd, FBIOGET_FSCREENINFO, &fix);
	printf(">>>>>[%s - %s:%d] before SetVideoSystem:\n"
				"                var.xres        : %4d, var.yres    : %4d, var.yres_virtual: %4d\n"
				"                fix.line_length : %4d, fix.smem_len: %d Byte\n",
				__path_file__, __func__, __LINE__,
				var.xres, var.yres, var.yres_virtual,
				fix.line_length, fix.smem_len);
#endif

	int ret = videoDecoder->SetVideoSystem(newSystem, remember);

#ifdef DEBUGINFO_SETVIDEOSYSTEM
	ioctl(fd, FBIOGET_VSCREENINFO, &var);
	ioctl(fd, FBIOGET_FSCREENINFO, &fix);
	printf(">>>>>[%s - %s:%d] after SetVideoSystem:\n"
				"                var.xres        : %4d, var.yres    : %4d, var.yres_virtual: %4d\n"
				"                fix.line_length : %4d, fix.smem_len: %d Byte\n",
				__path_file__, __func__, __LINE__,
				var.xres, var.yres, var.yres_virtual,
				fix.line_length, fix.smem_len);
#endif

	return ret;
}
