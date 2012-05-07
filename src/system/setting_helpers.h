#ifndef __setting_helpers__
#define __setting_helpers__

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


#include <gui/widget/menue.h>

#include <string>

class CGenericMenuActivate
{
	private:
		std::vector<CMenuItem*> items;
	public:
		CGenericMenuActivate()		{};
		~CGenericMenuActivate()		{ items.clear(); };

		void Add(CMenuItem* item)	{ items.push_back(item); }
		void Clear()			{ items.clear(); }
		void Activate(bool enable)
		{
			for(std::vector<CMenuItem*>::iterator it = items.begin(); it != items.end(); it++)
				(*it)->setActive(enable);
		}
};

class COnOffNotifier : public CChangeObserver
{
        private:
                int number;
                CMenuItem* toDisable[5];
        public:
                COnOffNotifier (CMenuItem* a1,CMenuItem* a2 = NULL,CMenuItem* a3 = NULL,CMenuItem* a4 = NULL,CMenuItem* a5 = NULL);
                bool changeNotify(const neutrino_locale_t, void *Data);
};

class CRecordingSafetyNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *);
};

class CMiscNotifier : public CChangeObserver
{
	private:
		CMenuItem* toDisable[2];
	public:
		CMiscNotifier( CMenuItem*, CMenuItem* );
		bool changeNotify(const neutrino_locale_t, void *);
};

class CLcdNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *);
};

class CPauseSectionsdNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void * Data);
};

class CSectionsdConfigNotifier : public CChangeObserver
{
        public:
                bool changeNotify(const neutrino_locale_t, void * );
};

class CTouchFileNotifier : public CChangeObserver
{
	const char * filename;
 public:
	inline CTouchFileNotifier(const char * file_to_modify)
		{
			filename = file_to_modify;
		};
	bool changeNotify(const neutrino_locale_t, void * data);
};

class CColorSetupNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *);
};

class CAudioSetupNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t OptionName, void *);
};

class CKeySetupNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *);
};

class CIPChangeNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void * Data);
};

class CTimingSettingsNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t OptionName, void *);
};

class CFontSizeNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void *);
};

class CRecAPIDSettingsNotifier : public CChangeObserver
{
	public:
	bool changeNotify(const neutrino_locale_t OptionName, void*);
};

class CSubtitleChangeExec : public CMenuTarget
{
	public:
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

void showSubchan(const std::string & subChannelName);
class CNVODChangeExec : public CMenuTarget
{
	public:
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CStreamFeaturesChangeExec : public CMenuTarget
{
	public:
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CMoviePluginChangeExec : public CMenuTarget
{
	public:
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class COnekeyPluginChangeExec : public CMenuTarget
{
	public:
		int exec(CMenuTarget* parent, const std::string & actionKey);
};
class CUCodeCheckExec : public CMenuTarget
{
	public:
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

int safe_mkdir(char * path);
int check_dir(const char * newdir);

class CTZChangeNotifier : public CChangeObserver
{
public:
        bool changeNotify(const neutrino_locale_t, void * data);
};

class CDataResetNotifier : public CMenuTarget
{
public:
        int exec(CMenuTarget* parent, const std::string& actionKey);
};

class CFanControlNotifier : public CChangeObserver
{
public:
        bool changeNotify(const neutrino_locale_t, void * data);
};

class CCpuFreqNotifier : public CChangeObserver
{
public:
        bool changeNotify(const neutrino_locale_t, void * data);
};
#if 0
class CScreenPresetNotifier : public CChangeObserver
{
public:
        bool changeNotify(const neutrino_locale_t, void * data);
};
#endif
class CAutoModeNotifier : public CChangeObserver
{
public:
        bool changeNotify(const neutrino_locale_t, void * data);
};

class CRadiotextNotifier : public CChangeObserver
{
	public:
		 bool changeNotify(const neutrino_locale_t, void * Data);
};

#endif
