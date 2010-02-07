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

#ifndef __plugins__
#define __plugins__

#include <gui/widget/menue.h>

#include <driver/framebuffer.h>
#include <system/localize.h>

#include <plugin.h>

#include <string>
#include <vector>

class CPlugins
{

	public:
	typedef enum p_type
	{
		P_TYPE_DISABLED = 0x1,
		P_TYPE_GAME     = 0x2,
		P_TYPE_TOOL     = 0x4,
		P_TYPE_SCRIPT   = 0x8
	}
	p_type_t;


	private:

		CFrameBuffer	*frameBuffer;

		struct plugin
		{
			std::string filename;
			std::string cfgfile;
			std::string pluginfile;
			int version;
			std::string name;                // UTF-8 encoded
			std::string description;         // UTF-8 encoded
			std::string depend;
			CPlugins::p_type_t type;

			bool fb;
			bool rc;
			bool lcd;
			bool vtxtpid;
			int posx, posy, sizex, sizey;
			bool showpig;
			bool needoffset;
			bool hide;
			bool operator< (const plugin& a) const
			{
				return this->filename < a.filename ;
			}
		};

		int fb, rc, lcd, pid;
		int number_of_plugins;

		std::vector<plugin> plugin_list;
		std::string plugin_dir;
		std::string scriptOutput;

		bool parseCfg(plugin *plugin_data);
		void scanDir(const char *dir);
		bool plugin_exists(const std::string & filename);
		int find_plugin(const std::string & filename);
		bool pluginfile_exists(const std::string & filename);
		CPlugins::p_type_t getPluginType(int type);
	public:

		~CPlugins();

		void loadPlugins();

		void setPluginDir(const std::string & dir) { plugin_dir = dir; }

		PluginParam * makeParam(const char * const id, const char * const value, PluginParam * const next);
		PluginParam * makeParam(const char * const id, const int          value, PluginParam * const next);

		inline       int           getNumberOfPlugins  (void            ) const { return plugin_list.size()                    ; }
		inline const char *        getName             (const int number) const { return plugin_list[number].name.c_str()      ; }
		inline const char *        getPluginFile       (const int number) const { return plugin_list[number].pluginfile.c_str(); }
		inline const char *        getFileName         (const int number) const { return plugin_list[number].filename.c_str()  ; }
		inline const std::string & getDescription      (const int number) const { return plugin_list[number].description       ; }
		inline       int           getType             (const int number) const { return plugin_list[number].type              ; }
		inline       bool          isHidden            (const int number) const { return plugin_list[number].hide              ; }

		void startPlugin(int number,int param);
		void start_plugin_by_name(const std::string & filename,int param);// start plugins by "name=" in .cfg
		void startScriptPlugin(int number);

		void startPlugin(const char * const filename); // start plugins also by name
		bool hasPlugin(CPlugins::p_type_t type);

		const std::string& getScriptOutput() const;
		void delScriptOutput();
};

#endif
