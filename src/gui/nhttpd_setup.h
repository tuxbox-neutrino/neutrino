/*
	Neutrino-GUI  -   DBoxII-Project

	nhttpd setup menu - Neutrino-GUI

	License: GPL
*/

#ifndef __nhttpd_setup__
#define __nhttpd_setup__

#include <gui/widget/menue.h>
#include <system/setting_helpers.h>

#include <string>

class CNhttpdSetup : public CMenuTarget, public CChangeObserver
{
	private:
		int width;
		int port;
		int auth_enabled;
		int threading;
		int loglevel;
		int ssl_enabled;

		std::string host;
		std::string username;
		std::string password;
		std::string no_auth_client;
		std::string ssl_pemfile;
		std::string ssl_ca_file;

		CGenericMenuActivate auth_items;
		CGenericMenuActivate ssl_items;

		void loadConfig();
		bool saveConfig();
		int showSetup();

	public:
		CNhttpdSetup();
		~CNhttpdSetup();

		int exec(CMenuTarget* parent, const std::string &actionKey);
		bool changeNotify(const neutrino_locale_t OptionName, void *Data);
};

#endif /* __nhttpd_setup__ */
