/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011-2014 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
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
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include <global.h>
#include <neutrino.h>

#include <system/helpers.h>

#include <zapit/client/zapittools.h>
#include "widget/shellwindow.h"

#include <poll.h>
#include <vector>

#include <hardware/video.h>
extern cVideo * videoDecoder;

#include "plugins.h"

#include <daemonc/remotecontrol.h>
#include <gui/lua/luainstance.h>

extern CPlugins       * g_Plugins;    /* neutrino.cpp */
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

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
			new_plugin.plugindir = dir;
			bool plugin_ok = parseCfg(&new_plugin);
			if (plugin_ok)
			{
				new_plugin.pluginfile = fname;
				if (new_plugin.type == CPlugins::P_TYPE_SCRIPT)
					new_plugin.pluginfile.append(".sh");
				else if (new_plugin.type == CPlugins::P_TYPE_LUA)
					new_plugin.pluginfile.append(".lua");
				else // CPlugins::P_TYPE_GAME or CPlugins::P_TYPE_TOOL
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
	scanDir(GAMESDIR);
	scanDir(g_settings.plugin_hdd_dir.c_str());
	scanDir(PLUGINDIR_MNT);
	scanDir(PLUGINDIR_VAR);
	scanDir(PLUGINDIR);

	sort (plugin_list.begin(), plugin_list.end());
}

CPlugins::~CPlugins()
{
	plugin_list.clear();
}

bool CPlugins::overrideType(plugin *plugin_data, std::string &setting, p_type type)
{
	if (!setting.empty()) {
		char s[setting.length() + 1];
		cstrncpy(s, setting, setting.length() + 1);
		char *t, *p = s;
		while ((t = strsep(&p, ",")))
			if (!strcmp(t, plugin_data->filename.c_str())) {
				plugin_data->type = type;
				return true;
			}
	}
	return false;
}

bool CPlugins::parseCfg(plugin *plugin_data)
{
	std::ifstream inFile;
	std::string line[20];
	int linecount = 0;
	bool reject = false;

	inFile.open(plugin_data->cfgfile.c_str());

	while (linecount < 20 && getline(inFile, line[linecount++]))
	{};

	plugin_data->index = sindex++;
	plugin_data->key = CRCInput::RC_nokey;
	plugin_data->name = "";
	plugin_data->description = "";
	plugin_data->shellwindow = false;
	plugin_data->hide = false;
	plugin_data->type = CPlugins::P_TYPE_DISABLED;
	plugin_data->integration = PLUGIN_INTEGRATION_DISABLED;
	plugin_data->hinticon = NEUTRINO_ICON_HINT_PLUGIN;

	std::string _hintIcon = plugin_data->plugindir + "/" + plugin_data->filename + "_hint.png";
	if (access(_hintIcon.c_str(), F_OK) == 0)
		plugin_data->hinticon = _hintIcon;

	for (int i = 0; i < linecount; i++)
	{
		std::istringstream iss(line[i]);
		std::string cmd;
		std::string parm;

		getline(iss, cmd, '=');
		getline(iss, parm, '=');

		if (cmd == "index")
		{
			plugin_data->index = atoi(parm);
		}
		else if (cmd == "key")
		{
			plugin_data->key = getPluginKey(parm);
		}
		else if (cmd == "name." + g_settings.language)
		{
			plugin_data->name = parm;
		}
		else if (cmd == "name")
		{
			if (plugin_data->name.empty())
				plugin_data->name = parm;
		}
		else if (cmd == "desc." + g_settings.language)
		{
			plugin_data->description = parm;
		}
		else if (cmd == "desc")
		{
			if (plugin_data->description.empty())
				plugin_data->description = parm;
		}
		else if (cmd == "depend")
		{
			plugin_data->depend = parm;
		}
		else if (cmd == "hinticon")
		{
			plugin_data->hinticon = parm;
		}
		else if (cmd == "type")
		{
			plugin_data->type = getPluginType(atoi(parm));
		}
		else if (cmd == "integration")
		{
			plugin_data->integration = atoi(parm);
		}
		else if (cmd == "shellwindow")
		{
			plugin_data->shellwindow = atoi(parm);
		}
		else if (cmd == "hide")
		{
			plugin_data->hide = atoi(parm);
		}
		else if (cmd == "needenigma")
		{
			reject = atoi(parm);
		}
	}

	inFile.close();

	if (plugin_data->name.empty())
		plugin_data->name = plugin_data->filename;

	_hintIcon = plugin_data->plugindir + "/" + plugin_data->hinticon + ".png";
	if (access(_hintIcon.c_str(), F_OK) == 0)
		plugin_data->hinticon = _hintIcon;

	overrideType(plugin_data, g_settings.plugins_disabled, P_TYPE_DISABLED) ||
	overrideType(plugin_data, g_settings.plugins_game, P_TYPE_GAME) ||
	overrideType(plugin_data, g_settings.plugins_tool, P_TYPE_TOOL) ||
	overrideType(plugin_data, g_settings.plugins_script, P_TYPE_SCRIPT) ||
	overrideType(plugin_data, g_settings.plugins_lua, P_TYPE_LUA);

	return !reject;
}

void CPlugins::startPlugin_by_name(const std::string & name)
{
	for (int i = 0; i <  (int) plugin_list.size(); i++)
	{
		if (name.compare(g_Plugins->getName(i)) == 0)
		{
			startPlugin(i);
			return;
		}
	}
}

void CPlugins::startPlugin(const char * const filename)
{
	int pluginnr = find_plugin(filename);
	if (pluginnr > -1)
		startPlugin(pluginnr);
	else
		printf("[CPlugins] could not find %s\n", filename);
}

void CPlugins::popenScriptPlugin(const char * script)
{
	pid_t pid = 0;
	FILE *f = my_popen(pid, script, "r");
	if (f != NULL)
	{
		char *output=NULL;
		size_t len = 0;
		while ((getline(&output, &len, f)) != -1)
			scriptOutput += output;
		pclose(f);
		int s;
		while (waitpid(pid, &s, WNOHANG)>0);
		kill(pid, SIGTERM);
		if (output)
			free(output);
	}
	else
		printf("[CPlugins] can't execute %s\n",script);
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

	// workaround for manually messed up permissions
	if (access(script, X_OK))
		chmod(script, 0755);
	if (plugin_list[number].shellwindow)
	{
		int res = 0;
		CShellWindow (script, CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE, &res);
		scriptOutput = "";
	}
	else
		popenScriptPlugin(script);
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
#ifdef ENABLE_LUA
	CLuaInstance *lua = new CLuaInstance();
	lua->runScript(script);
	delete lua;
#endif
#if 0
	frameBuffer->ClearFB();
#endif
	videoDecoder->Pig(-1, -1, -1, -1);
	frameBuffer->paintBackground();
}

void CPlugins::startPlugin(int number)
{
	// always delete old output
	delScriptOutput();
	/* export neutrino settings to the environment */
	char tmp[32];
#if 0
	sprintf(tmp, "%d", g_settings.screen_StartX_int);
#else
	sprintf(tmp, "%d", g_settings.screen_StartX);
#endif
	setenv("SCREEN_OFF_X", tmp, 1);
#if 0
	sprintf(tmp, "%d", g_settings.screen_StartY_int);
#else
	sprintf(tmp, "%d", g_settings.screen_StartY);
#endif
	setenv("SCREEN_OFF_Y", tmp, 1);
#if 0
	sprintf(tmp, "%d", g_settings.screen_EndX_int);
#else
	sprintf(tmp, "%d", g_settings.screen_EndX);
#endif
	setenv("SCREEN_END_X", tmp, 1);
#if 0
	sprintf(tmp, "%d", g_settings.screen_EndY_int);
#else
	sprintf(tmp, "%d", g_settings.screen_EndY);
#endif
	setenv("SCREEN_END_Y", tmp, 1);

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

	g_RCInput->clearRCMsg();
	g_RCInput->stopInput();
	/* stop automatic updates etc. */
	frameBuffer->Lock();
	//frameBuffer->setMode(720, 576, 8 * sizeof(fb_pixel_t));
	printf("Starting %s\n", plugin_list[number].pluginfile.c_str());

	// workaround for manually messed up permissions
	if (access(plugin_list[number].pluginfile, X_OK))
		chmod(plugin_list[number].pluginfile.c_str(), 0755);

	my_system(2, plugin_list[number].pluginfile.c_str(), NULL);
	//frameBuffer->setMode(720, 576, 8 * sizeof(fb_pixel_t));
	frameBuffer->Unlock();
#if 0
	frameBuffer->ClearFB();
#endif
	videoDecoder->Pig(-1, -1, -1, -1);
	frameBuffer->paintBackground();
	g_RCInput->restartInput();
	g_RCInput->clearRCMsg();
}

bool CPlugins::hasPlugin(CPlugins::p_type_t type)
{
	for (std::vector<plugin>::iterator it=plugin_list.begin();
			it!=plugin_list.end(); ++it)
	{
		if ((it->type & type) && !it->hide)
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

neutrino_msg_t CPlugins::getPluginKey(std::string key)
{
	if (key == "red")
		return CRCInput::RC_red;
	else if (key == "green")
		return CRCInput::RC_green;
	else if (key == "yellow")
		return CRCInput::RC_yellow;
	else if (key == "blue")
		return CRCInput::RC_blue;
	else /* (key == "auto") */
		return CRCInput::RC_nokey;
}
