
#ifndef __osd_helpers__
#define __osd_helpers__

#include <sigc++/signal.h>

enum {
	OSDMODE_720  = 0,
	OSDMODE_1080 = 1
};

class COsdHelpers : public sigc::trackable
{
	private:

	public:
		COsdHelpers();
		~COsdHelpers();
		static COsdHelpers* getInstance();

		int g_settings_osd_resolution_save;

		void changeOsdResolution(uint32_t mode, bool automode=false, bool forceOsdReset=false);
		int  isVideoSystem1080(int res);
		int  getVideoSystem();
		uint32_t getOsdResolution();
		int setVideoSystem(int newSystem, bool remember = true);
		sigc::signal<void> OnAfterChangeResolution;
		sigc::signal<void> OnBeforeChangeResolution;
};


#endif //__osd_helpers__
