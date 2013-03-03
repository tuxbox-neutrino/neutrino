/*
        Neutrino-GUI  -   DBoxII-Project

        Copyright (C) 2011 CoolStream International Ltd

        License: GPLv2

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation;

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include <gui/widget/icons.h>
#include "network_service.h"
#include <mymenu.h>

#include <driver/screen_max.h>

#include <system/debug.h>
#include <system/helpers.h>

#include <unistd.h>

#define TOUCH_BASE "/var/etc/."

struct network_service
{
	std::string name;
	std::string cmd;
	std::string options;
	neutrino_locale_t hint;
	std::string icon;
	int enabled;
};

#define SERVICE_COUNT 4
static struct network_service services[SERVICE_COUNT] =
{
	{ "FTP", "vsftpd", "", LOCALE_MENU_HINT_NET_FTPD, "", 0 },
	{ "Telnet", "telnetd", "-l/bin/login", LOCALE_MENU_HINT_NET_TELNET, "", 0 },
	{ "DjMount", "djmount", "-o iocharset=utf8 /media/00upnp/", LOCALE_MENU_HINT_NET_DJMOUNT, "", 0 },
	{ "uShare", "ushare", "-D -n `cat /etc/hostname`", LOCALE_MENU_HINT_NET_USHARE, "", 0 }
};

CNetworkService::CNetworkService(std::string cmd, std::string opts)
{
	command = cmd;
	options = opts;
	enabled = false;

	std::string file = TOUCH_BASE + cmd;
	if (!access(file.c_str(), F_OK))
		enabled = true;
}

void CNetworkService::Start()
{
	std::string cmd = command + " " + options;
	printf("CNetworkService::Start: %s %s\n", command.c_str(), options.c_str());
	my_system(3, "/bin/sh", "-c", cmd.c_str());
	enabled = true;
	TouchFile();
}

void CNetworkService::Stop()
{
	const char killall []= "killall";
	printf("CNetworkService::Stop: %s %s\n", killall, command.c_str());
	my_system(2, killall, command.c_str());
	enabled = false;
	TouchFile();
}

void CNetworkService::TouchFile()
{
	std::string file = TOUCH_BASE + command;
	printf("CNetworkService::TouchFile: %s %s\n", enabled ? "create" : "remove", file.c_str());
	if(enabled) {
		FILE * fp = fopen(file.c_str(), "w");
		if (fp)
			fclose(fp);
	} else {
		unlink(file.c_str());
	}
}

bool CNetworkService::changeNotify(const neutrino_locale_t /*OptionName*/, void * data)
{
	int value = *(int *)data;

	printf("CNetworkService::changeNotify: %d (enabled %d)\n", value, enabled);
	if (value != 0)
		Start();
	else
		Stop();

	return false;
}

CNetworkServiceSetup::CNetworkServiceSetup()
{
	width = w_max (40, 10);
	selected = -1;
}

CNetworkServiceSetup::~CNetworkServiceSetup()
{
}

int CNetworkServiceSetup::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init network services setup menu\n");

	if (parent)
		parent->hide();

	return showNetworkServiceSetup();
}

int CNetworkServiceSetup::showNetworkServiceSetup()
{
	int shortcut = 1;

	CMenuWidget* setup = new CMenuWidget(LOCALE_MAINSETTINGS_NETWORK, NEUTRINO_ICON_SETTINGS, width);
	setup->setSelected(selected);
	setup->addIntroItems(LOCALE_NETWORKMENU_SERVICES);

	CNetworkService * items[SERVICE_COUNT];

	//telnetd used inetd
	bool useinetd = false;
	char *buf=NULL;
	size_t len = 0;

	FILE* fd = fopen("/etc/inetd.conf", "r");
	    if(fd)
	    {
		while(!feof(fd))
		{
			if(getline(&buf, &len, fd)!=-1)
			{
				if (strstr(buf, "telnetd") != NULL)
				{
					useinetd = true;
					break;
				}
			}
		}
		fclose(fd);
		if(buf)
			free(buf);
	    }

	//set active when found
	bool active;
		
	for(unsigned i = 0; i < SERVICE_COUNT; i++) {
		items[i] = new CNetworkService(services[i].cmd, services[i].options);
		services[i].enabled = items[i]->Enabled();

		std::string execute1 = "/bin/" + services[i].cmd;
		std::string execute2 = "/sbin/" + services[i].cmd;

		active = false;
		if ( !(access(execute1.c_str(), F_OK)) || !(access(execute2.c_str(), F_OK)) )
			active = true;
			
		if ( (services[i].name == "Telnet") && useinetd)
			active = false;
		
		CMenuOptionChooser * mc = new CMenuOptionChooser(services[i].name.c_str(), &services[i].enabled, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, active, items[i], CRCInput::convertDigitToKey(shortcut), "");
		if (active)
			shortcut++;

		mc->setHint(services[i].icon, services[i].hint);
		setup->addItem(mc);
	}

	int res = setup->exec (NULL, "");
	selected = setup->getSelected();
	delete setup;

	for(unsigned i = 0; i < SERVICE_COUNT; i++)
		delete items[i];

	return res;
}
