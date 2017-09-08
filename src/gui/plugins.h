/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

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

#ifndef __plugins__
#define __plugins__

#include <system/localize.h>

#include <plugin.h>

#include <string>
#include <vector>

class CFrameBuffer;
class CPlugins
{
	public:
		// neutrino-internal plugin-type conversion
		typedef enum p_type
		{
			P_TYPE_DISABLED	= 0x1,
			P_TYPE_GAME	= 0x2,
			P_TYPE_TOOL	= 0x4,
			P_TYPE_SCRIPT	= 0x8,
			P_TYPE_LUA	= 0x10
		}
		p_type_t;

	private:
		CFrameBuffer	*frameBuffer;

		struct plugin
		{
			int index;
			std::string filename;
			neutrino_msg_t key;
			std::string cfgfile;
			std::string pluginfile;
			std::string plugindir;
			std::string hinticon;
			int version;
			std::string name;                // UTF-8 encoded
			std::string description;         // UTF-8 encoded
			std::string depend;
			CPlugins::p_type_t type;
			int integration;
			bool shellwindow;
			bool hide;
			bool operator< (const plugin& a) const
			{
				return this->index < a.index ;
			}
		};

		int number_of_plugins;
		int sindex;
		std::vector<plugin> plugin_list;
		std::string plugin_dir;
		std::string scriptOutput;

		bool parseCfg(plugin *plugin_data);
		void scanDir(const char *dir);
		bool plugin_exists(const std::string & filename);
		int find_plugin(const std::string & filename);
		CPlugins::p_type_t getPluginType(int type);
		neutrino_msg_t getPluginKey(std::string key="auto");

	public:
		CPlugins();
		~CPlugins();

		void loadPlugins();

		void setPluginDir(const std::string & dir) { plugin_dir = dir; }

		inline       int           getNumberOfPlugins  (void            ) const { return plugin_list.size()                    ; }
		inline const char *        getName             (const int number) const { return plugin_list[number].name.c_str()      ; }
		inline const char *        getPluginFile       (const int number) const { return plugin_list[number].pluginfile.c_str(); }
		inline const char *        getPluginDir        (const int number) const { return plugin_list[number].plugindir.c_str() ; }
		inline const char *        getHintIcon         (const int number) const { return plugin_list[number].hinticon.c_str()  ; }
		inline const char *        getFileName         (const int number) const { return plugin_list[number].filename.c_str()  ; }
		inline const std::string & getDescription      (const int number) const { return plugin_list[number].description       ; }
		inline       int           getType             (const int number) const { return plugin_list[number].type              ; }
		inline       int           getIntegration      (const int number) const { return plugin_list[number].integration       ; }
		inline       bool          isHidden            (const int number) const { return plugin_list[number].hide              ; }
		inline       int           getIndex            (const int number) const { return plugin_list[number].index             ; }
		inline      neutrino_msg_t getKey              (const int number) const { return plugin_list[number].key               ; }

		void setType (const int number, int t) { plugin_list[number].type = (CPlugins::p_type_t) t ; }
		bool overrideType(plugin *plugin_data, std::string &setting, p_type type);

		void startPlugin(int number);				// start plugins by number
		void startPlugin(const char * const filename);		// start plugins by filename
		void startPlugin_by_name(const std::string & name);	// start plugins by "name=" in .cfg
		void startScriptPlugin(int number);
		void popenScriptPlugin(const char * script);
		void startLuaPlugin(int number);
		bool hasPlugin(CPlugins::p_type_t type);

		const std::string& getScriptOutput() const;
		void delScriptOutput();
};

#endif
