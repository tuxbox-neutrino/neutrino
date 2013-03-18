/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

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

#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>

#include <dirent.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include <global.h>
#include <neutrino.h>

#include <system/helpers.h>

#include <zapit/client/zapittools.h>

#include "plugins.h"
/* for alexW images with old drivers:
 * #define USE_VBI_INTERFACE 1
 */

#ifdef USE_VBI_INTERFACE
#define AVIA_VBI_START_VTXT	1
#define AVIA_VBI_STOP_VTXT	2
#endif

#include <daemonc/remotecontrol.h>
#include <gui/luainstance.h>

extern CPlugins       * g_PluginList;    /* neutrino.cpp */
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

#define PLUGINDIR_VAR "/var/tuxbox/plugins"
#define PLUGINDIR_USB "/mnt/usb/tuxbox/plugins"

CPlugins::CPlugins()
{
	frameBuffer = NULL;
	number_of_plugins = 0;
}

bool CPlugins::plugin_exists(const std::string & filename)
{
	return (find_plugin(filename) >= 0);
}

int CPlugins::find_plugin(const std::string & filename)
{
	for (int i = 0; i <  (int) plugin_list.size(); i++)
	{
		if ( (filename.compare(plugin_list[i].filename) == 0) || (filename.compare(plugin_list[i].filename + ".cfg") == 0) )
			return i;
	}
	return -1;
}

void CPlugins::scanDir(const char *dir)
{
	struct dirent **namelist;
	std::string fname;

	int number_of_files = scandir(dir, &namelist, 0, alphasort);

	if (number_of_files < 0)
		return;

	for (int i = 0; i < number_of_files; i++)
	{
		std::string filename(namelist[i]->d_name);
		free(namelist[i]);

		int pos = filename.find(".cfg");
		if (pos > -1)
		{
			plugin new_plugin;
			new_plugin.filename = filename.substr(0, pos);
			fname = dir;
			fname += '/';
			new_plugin.cfgfile = fname.append(new_plugin.filename);
			new_plugin.cfgfile.append(".cfg");
			bool plugin_ok = parseCfg(&new_plugin);
			if (plugin_ok)
			{
				new_plugin.pluginfile = fname;
				if (new_plugin.type == CPlugins::P_TYPE_SCRIPT)
					new_plugin.pluginfile.append(".sh");
				else if (new_plugin.type == CPlugins::P_TYPE_LUA)
					new_plugin.pluginfile.append(".lua");
				else
					new_plugin.pluginfile.append(".so");
				// We do not check if new_plugin.pluginfile exists since .cfg in
				// PLUGINDIR_VAR can overwrite settings in read only dir
				// PLUGINDIR. This needs PLUGINDIR_VAR to be scanned at
				// first -> .cfg in PLUGINDIR will be skipped since plugin
				// already exists in the list.
				// This behavior is used to make sure plugins can be disabled
				// by creating a .cfg in PLUGINDIR_VAR (PLUGINDIR often is read only).

				if (!plugin_exists(new_plugin.filename))
				{
					plugin_list.push_back(new_plugin);
					number_of_plugins++;
				}
			}
		}
	}
	free(namelist);
}

void CPlugins::loadPlugins()
{
	frameBuffer = CFrameBuffer::getInstance();
	number_of_plugins = 0;
	plugin_list.clear();
	sindex = 100;
	scanDir(g_settings.plugin_hdd_dir.c_str());
	scanDir(PLUGINDIR_USB);
	scanDir(PLUGINDIR_VAR);
	scanDir(PLUGINDIR);

	sort (plugin_list.begin(), plugin_list.end());
}

CPlugins::~CPlugins()
{
	plugin_list.clear();
}

bool CPlugins::parseCfg(plugin *plugin_data)
{
//	FILE *fd;

	std::ifstream inFile;
	std::string line[20];
	int linecount = 0;
	bool reject = false;

	inFile.open(plugin_data->cfgfile.c_str());

	while (linecount < 20 && getline(inFile, line[linecount++]))
	{};

	plugin_data->index = sindex++;
	plugin_data->key = 0; //CRCInput::RC_nokey
	plugin_data->fb = false;
	plugin_data->rc = false;
	plugin_data->lcd = false;
	plugin_data->vtxtpid = false;
	plugin_data->showpig = false;
	plugin_data->needoffset = false;
	plugin_data->hide = false;
	plugin_data->type = CPlugins::P_TYPE_DISABLED;

	for (int i = 0; i < linecount; i++)
	{
		std::istringstream iss(line[i]);
		std::string cmd;
		std::string parm;

		getline(iss, cmd, '=');
		getline(iss, parm, '=');

		if (cmd == "index")
		{
			plugin_data->index = atoi(parm.c_str());
		}
		else if (cmd == "pluginversion")
		{
			plugin_data->key = atoi(parm.c_str());
		}
		else if (cmd == "name")
		{
			plugin_data->name = parm;
		}
		else if (cmd == "desc")
		{
			plugin_data->description = parm;
		}
		else if (cmd == "depend")
		{
			plugin_data->depend = parm;
		}
		else if (cmd == "type")
		{
			plugin_data->type = getPluginType(atoi(parm.c_str()));
		}
		else if (cmd == "needfb")
		{
			plugin_data->fb = ((parm == "1")?true:false);
		}
		else if (cmd == "needrc")
		{
			plugin_data->rc = ((parm == "1")?true:false);
		}
		else if (cmd == "needlcd")
		{
			plugin_data->lcd = ((parm == "1")?true:false);
		}
		else if (cmd == "needvtxtpid")
		{
			plugin_data->vtxtpid = ((parm == "1")?true:false);
		}
		else if (cmd == "pigon")
		{
			plugin_data->showpig = ((parm == "1")?true:false);
		}
		else if (cmd == "needoffsets")
		{
			plugin_data->needoffset = ((parm == "1")?true:false);
		}
		else if (cmd == "hide")
		{
			plugin_data->hide = ((parm == "1")?true:false);
		}
		else if (cmd == "needenigma")
		{
			reject = ((parm == "1")?true:false);
		}

	}

	inFile.close();
	return !reject;
}

PluginParam * CPlugins::makeParam(const char * const id, const char * const value, PluginParam * const next)
{
	PluginParam * startparam = new PluginParam;

	startparam->next = next;
	startparam->id   = id;
	startparam->val  = strdup(value);

	return startparam;
}

PluginParam * CPlugins::makeParam(const char * const id, const int value, PluginParam * const next)
{
	char aval[10];

	sprintf(aval, "%d", value);

	return makeParam(id, aval, next);
}

void CPlugins::start_plugin_by_name(const std::string & filename,int param)
{
	for (int i = 0; i <  (int) plugin_list.size(); i++)
	{
		if (filename.compare(g_PluginList->getName(i))==0)
		{
			startPlugin(i,param);
			return;
		}
	}
}

void CPlugins::startPlugin(const char * const name)
{
	int pluginnr = find_plugin(name);
	if (pluginnr > -1)
		startPlugin(pluginnr,0);
	else
		printf("[CPlugins] could not find %s\n", name);

}

void CPlugins::startScriptPlugin(int number)
{
	const char *script = plugin_list[number].pluginfile.c_str();
	printf("[CPlugins] executing script %s\n",script);
	if (!file_exists(script))
	{
		printf("[CPlugins] could not find %s,\nperhaps wrong plugin type in %s\n",
		       script, plugin_list[number].cfgfile.c_str());
		return;
	}
	pid_t pid = 0;
	FILE *f = my_popen(pid,script,"r");
	if (f != NULL)
	{
		char *output=NULL;
		size_t len = 0;
		while (( getline(&output, &len, f)) != -1)

		{
			scriptOutput += output;
		}
		pclose(f);
		int s;
		while (waitpid(pid,&s,WNOHANG)>0);
		kill(pid,SIGTERM);
		if(output)
			free(output);
	}
	else
	{
		printf("[CPlugins] can't execute %s\n",script);
	}
}

void CPlugins::startLuaPlugin(int number)
{
	const char *script = plugin_list[number].pluginfile.c_str();
	printf("[CPlugins] executing lua script %s\n",script);
	if (!file_exists(script))
	{
		printf("[CPlugins] could not find %s,\nperhaps wrong plugin type in %s\n",
		       script, plugin_list[number].cfgfile.c_str());
		return;
	}
	CLuaInstance *lua = new CLuaInstance();
	lua->runScript(script);
	delete lua;
}

void CPlugins::startPlugin(int number,int /*param*/)
{
	// always delete old output
	delScriptOutput();
	/* export neutrino settings to the environment */
	char tmp[32];
	sprintf(tmp, "%d", g_settings.screen_StartX);
	setenv("SCREEN_OFF_X", tmp, 1);
	sprintf(tmp, "%d", g_settings.screen_StartY);
	setenv("SCREEN_OFF_Y", tmp, 1);
	sprintf(tmp, "%d", g_settings.screen_EndX);
	setenv("SCREEN_END_X", tmp, 1);
	sprintf(tmp, "%d", g_settings.screen_EndY);
	setenv("SCREEN_END_Y", tmp, 1);

	//bool ispip = strncmp(plugin_list[number].pluginfile.c_str(), "pip", 3) ? false : true;
	bool ispip  = strstr(plugin_list[number].pluginfile.c_str(), "pip") != 0;
//printf("exec: %s pip: %d\n", plugin_list[number].pluginfile.c_str(), ispip);
	if (ispip && !g_RemoteControl->is_video_started)
		return;
	if (plugin_list[number].type == CPlugins::P_TYPE_SCRIPT)
	{
		startScriptPlugin(number);
		return;
	}
	if (plugin_list[number].type == CPlugins::P_TYPE_LUA)
	{
		startLuaPlugin(number);
		return;
	}
	if (!file_exists(plugin_list[number].pluginfile.c_str()))
	{
		printf("[CPlugins] could not find %s,\nperhaps wrong plugin type in %s\n",
		       plugin_list[number].pluginfile.c_str(), plugin_list[number].cfgfile.c_str());
		return;
	}

#if 0
	PluginExec execPlugin;
	char depstring[129];
	char			*argv[20];
	void			*libhandle[20];
	int			argc = 0, i = 0, lcd_fd=-1;
	char			*p;
	char			*np;
	void			*handle;
	char *        error;
	int           vtpid      =  0;
	PluginParam * startparam =  0;
#endif
	g_RCInput->clearRCMsg();
#if 0
	if (plugin_list[number].fb)
	{
		startparam = makeParam(P_ID_FBUFFER  , frameBuffer->getFileHandle()    , startparam);
	}
	if (plugin_list[number].rc)
	{
		startparam = makeParam(P_ID_RCINPUT  , g_RCInput->getFileHandle()      , startparam);
		startparam = makeParam(P_ID_RCBLK_ANF, g_settings.repeat_genericblocker, startparam);
		startparam = makeParam(P_ID_RCBLK_REP, g_settings.repeat_blocker       , startparam);
	}
	else
	{
		g_RCInput->stopInput();
	}
	if (plugin_list[number].lcd)
	{
		CLCD::getInstance()->pause();

		lcd_fd = open("/dev/dbox/lcd0", O_RDWR);

		startparam = makeParam(P_ID_LCD      , lcd_fd                          , startparam);
	}
	if (plugin_list[number].vtxtpid)
	{
		vtpid = g_RemoteControl->current_PIDs.PIDs.vtxtpid;
#ifdef USE_VBI_INTERFACE
		int fd = open("/dev/dbox/vbi0", O_RDWR);
		if (fd > 0)
		{
			ioctl(fd, AVIA_VBI_STOP_VTXT, 0);
			close(fd);
		}
#endif
		if (param>0)
			vtpid=param;
		startparam = makeParam(P_ID_VTXTPID, vtpid, startparam);
	}
	if (plugin_list[number].needoffset)
	{
		startparam = makeParam(P_ID_VFORMAT  , g_settings.video_Format         , startparam);
		startparam = makeParam(P_ID_OFF_X    , g_settings.screen_StartX        , startparam);
		startparam = makeParam(P_ID_OFF_Y    , g_settings.screen_StartY        , startparam);
		startparam = makeParam(P_ID_END_X    , g_settings.screen_EndX          , startparam);
		startparam = makeParam(P_ID_END_Y    , g_settings.screen_EndY          , startparam);
	}

	PluginParam *par = startparam;
	for ( ; par; par=par->next )
	{
		printf("[CPlugins] (id,val):(%s,%s)\n", par->id, par->val);
	}
	std::string pluginname = plugin_list[number].filename;

	strcpy(depstring, plugin_list[number].depend.c_str());

	argc=0;
	if ( depstring[0] )
	{
		p=depstring;
		while ( 1 )
		{
			argv[ argc ] = p;
			argc++;
			np = strchr(p,',');
			if ( !np )
				break;

			*np=0;
			p=np+1;
			if ( argc == 20 )	// mehr nicht !
				break;
		}
	}
	for ( i=0; i<argc; i++ )
	{
		std::string libname = argv[i];
		printf("[CPlugins] try load shared lib : %s\n",argv[i]);
		libhandle[i] = dlopen ( *argv[i] == '/' ?
					argv[i] : (PLUGINDIR "/"+libname).c_str(),
					RTLD_NOW | RTLD_GLOBAL );
		if ( !libhandle[i] )
		{
			fputs (dlerror(), stderr);
			break;
		}
	}
	if ( i == argc )		// alles geladen
	{
		//bool ispip = strncmp(plugin_list[number].pluginfile.c_str(), "pip", 3) ? true : false;
		handle = dlopen ( plugin_list[number].pluginfile.c_str(), RTLD_NOW);
		if (!handle)
		{
			fputs (dlerror(), stderr);
		} else {
			execPlugin = (PluginExec) dlsym(handle, "plugin_exec");
			if ((error = dlerror()) != NULL)
			{
				fputs(error, stderr);
				dlclose(handle);
			} else {
				printf("[CPlugins] try exec...\n");
				if (ispip) {
					g_Sectionsd->setPauseScanning (true);
					g_Zapit->setEventMode(false);
					if (g_Zapit->isPlayBackActive()) {
						if (!CNeutrinoApp::getInstance()->recordingstatus)
							g_Zapit->setRecordMode(true);
					} else {
						/* no playback, we playing file ? zap to channel */
						t_channel_id current_channel = g_Zapit->getCurrentServiceID();
						g_Zapit->zapTo_serviceID(current_channel);
					}
				}
				g_RCInput->close_click();
#ifndef FB_USE_PALETTE
				if (plugin_list[number].fb) {
					frameBuffer->setMode(720, 576, 8);
					frameBuffer->paletteSet();
				}
#endif
				execPlugin(startparam);
				dlclose(handle);
				printf("[CPlugins] exec done...\n");
				g_RCInput->open_click();
			}
		}
		//restore framebuffer...
//#if 0
//		g_RCInput->open_click();
//#endif

		//if (!plugin_list[number].rc)
		g_RCInput->restartInput();
		g_RCInput->clearRCMsg();

		if (plugin_list[number].lcd)
		{
			if (lcd_fd != -1)
				close(lcd_fd);
			CLCD::getInstance()->resume();
		}

		if (plugin_list[number].fb)
		{
#ifdef FB_USE_PALETTE
			frameBuffer->paletteSet();
#else
			frameBuffer->setMode(720, 576, 8 * sizeof(fb_pixel_t));
#endif
		}
		frameBuffer->paintBackgroundBox(0,0,720,576);
		if (ispip) {
			if (!CNeutrinoApp::getInstance()->recordingstatus) {
				g_Zapit->setRecordMode(false);
			}
			g_Zapit->setEventMode(true);
			g_Sectionsd->setPauseScanning (false);
		}

#ifdef USE_VBI_INTERFACE
		if (plugin_list[number].vtxtpid)
		{
			if (vtpid != 0)
			{
				// versuche, den gtx/enx_vbi wieder zu starten
				int fd = open("/dev/dbox/vbi0", O_RDWR);
				if (fd > 0)
				{
					ioctl(fd, AVIA_VBI_START_VTXT, vtpid);
					close(fd);
				}
			}
		}
#endif
	}

	/* unload shared libs */
	for ( i=0; i<argc; i++ )
	{
		if ( libhandle[i] )
			dlclose(libhandle[i]);
		else
			break;
	}

	for (par = startparam ; par; )
	{
		/* we must not free par->id, since it is the original */
		free(par->val);
		PluginParam * tmp = par;
		par = par->next;
		delete tmp;
	}
	g_RCInput->clearRCMsg();
#else
	g_RCInput->clearRCMsg();
	g_RCInput->stopInput();
	/* stop automatic updates etc. */
	frameBuffer->Lock();
	//frameBuffer->setMode(720, 576, 8 * sizeof(fb_pixel_t));
	printf("Starting %s\n", plugin_list[number].pluginfile.c_str());
	my_system(2, plugin_list[number].pluginfile.c_str(), NULL);
	//frameBuffer->setMode(720, 576, 8 * sizeof(fb_pixel_t));
	frameBuffer->Unlock();
	frameBuffer->paintBackground();
	g_RCInput->restartInput();
	g_RCInput->clearRCMsg();
#endif
}

bool CPlugins::hasPlugin(CPlugins::p_type_t type)
{
	for (std::vector<plugin>::iterator it=plugin_list.begin();
			it!=plugin_list.end(); ++it)
	{
		if (it->type == type && !it->hide)
			return true;
	}
	return false;
}

const std::string& CPlugins::getScriptOutput() const
{
	return scriptOutput;
}

void CPlugins::delScriptOutput()
{
	scriptOutput.clear();
}

CPlugins::p_type_t CPlugins::getPluginType(int type)
{
	switch (type)
	{
	case PLUGIN_TYPE_DISABLED:
		return P_TYPE_DISABLED;
		break;
	case PLUGIN_TYPE_GAME:
		return P_TYPE_GAME;
		break;
	case PLUGIN_TYPE_TOOL:
		return P_TYPE_TOOL;
		break;
	case PLUGIN_TYPE_SCRIPT:
		return P_TYPE_SCRIPT;
		break;
	case PLUGIN_TYPE_LUA:
		return P_TYPE_LUA;
	default:
		return P_TYPE_DISABLED;
	}
}


