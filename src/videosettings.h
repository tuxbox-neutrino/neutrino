#ifndef _VIDEO_SETTINGS_
#define _VIDEO_SETTINGS_

class CVideoSettings : public CMenuWidget, CChangeObserver
{
	CMenuForwarder *   SyncControlerForwarder;
	CMenuOptionChooser * VcrVideoOutSignalOptionChooser;
	CRGBCSyncControler RGBCSyncControler;
	int                vcr_video_out_signal;

public:
	CVideoSettings();
	virtual bool changeNotify(const neutrino_locale_t OptionName, void *);
	virtual void paint();
	void nextMode(void);
	void next43Mode(void);
	void SwitchFormat(void);
};
#endif
