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

#include <system/localize.h>
#include <system/locals_intern.h>

#include <cstring>
#include <fstream>
#include <string>
#include <iostream>
#include <map>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//static const char * iso639filename = "/usr/share/iso-codes/iso-639.tab";
static const char * iso639filename = "/share/iso-codes/iso-639.tab";

#define DEFAULT_LOCALE "english"

std::map<std::string, std::string> iso639;
std::map<std::string, std::string> iso639rev;

void initialize_iso639_map(void)
{
	std::string s, t, u, v;
	std::ifstream in(iso639filename);
	if (in.is_open())
	{
		while (in.peek() == '#')
			getline(in, s);
		while (in >> s >> t >> u >> std::ws)
		{
			getline(in, v);
			iso639[s] = v;
			if (s != t) {
				iso639[t] = v;
			}
			iso639rev[v] = s;
		}
	}
	else
		std::cout << "Loading " << iso639filename << " failed." << std::endl;
}

const char * getISO639Description(const char * const iso)
{
	std::map<std::string, std::string>::const_iterator it = iso639.find(std::string(iso));
	if (it == iso639.end())
		return iso;
	else
		return it->second.c_str();
}

CLocaleManager::CLocaleManager()
{
	localeData = new char * [sizeof(locale_real_names)/sizeof(const char *)];
	defaultData = new char * [sizeof(locale_real_names)/sizeof(const char *)];
	memcpy(localeData, locale_real_names, sizeof(locale_real_names));
	memcpy(defaultData, locale_real_names, sizeof(locale_real_names));
	defaultDataMem = localeDataMem = NULL;

	loadLocale(DEFAULT_LOCALE, true);
}

CLocaleManager::~CLocaleManager()
{
	delete[] localeData;
	delete[] defaultData;

	if (localeDataMem)
		::free(localeDataMem);
	if (defaultDataMem)
		::free(defaultDataMem);
}

#define LOCALEDIR_VAR "/var/tuxbox/locale"
const char * path[2] = { LOCALEDIR_VAR, DATADIR "/neutrino/locale/"};

CLocaleManager::loadLocale_ret_t CLocaleManager::loadLocale(const char * const locale, bool asdefault)
{
	FILE * fd = NULL;
	char ** loadData = asdefault ? defaultData : localeData;

	char **mem = asdefault ? &defaultDataMem : &localeDataMem;

	if(!asdefault && !strcmp(locale, DEFAULT_LOCALE)) {
		if (*mem) {
			free(*mem);
			*mem = NULL;
		}
		memcpy(loadData, defaultData, sizeof(locale_real_names));
		return UNICODE_FONT;
	}

	struct stat st;
	for (unsigned int i = 0; i < 2; i++)
	{
		std::string filename = path[i];
		filename += locale;
		filename += ".locale";
		::stat(filename.c_str(), &st);
		
		fd = fopen(filename.c_str(), "r");
		if (fd)
			break;
	}
	
	if (!fd)
	{		
		perror("cannot read locale");
		return NO_SUCH_LOCALE;
	}

	if (*mem) {
		free (*mem);
		*mem = NULL;
	}

	memcpy(loadData, locale_real_names, sizeof(locale_real_names));

	*mem = (char *) malloc(st.st_size);
	if (!*mem)
	{
		perror("loadLocale");
		return NO_SUCH_LOCALE;
	}
	char *memp = *mem;

	char *buf=NULL;
	size_t len = 0;

	while(!feof(fd))
	{
		if(getline(&buf, &len, fd)!=-1)
		{
			char * val    = NULL;
			char * tmpptr = buf;

			for(; (*tmpptr!=10) && (*tmpptr!=13);tmpptr++)
			{
				if ((*tmpptr == ' ') && (val == NULL))
				{
					*tmpptr  = 0;
					val      = tmpptr + 1;
				}
			}
			*tmpptr = 0;

			if (val == NULL)
				continue;

			std::string text = val;

			int pos;
			do
			{
				pos = text.find("\\n");
				if ( pos!=-1 )
				{
					text.replace(pos, 2, "\n", 1);
				}
			} while ( ( pos != -1 ) );

			unsigned int i;
			for(i = 1; i < sizeof(locale_real_names)/sizeof(const char *); i++)
			{
//printf("[%s] [%s]\n", buf,locale_real_names[i]);
				if(!strcmp(buf,locale_real_names[i]))
				{
					if(loadData[i] == locale_real_names[i])
					{
						loadData[i] = memp;
						size_t l = text.length() + 1;
						memcpy(memp, text.c_str(), l);
						memp += l;
					}
					else
						printf("[%s.locale] dup entry: %s\n", locale, locale_real_names[i]);
					break;
				}
			}
//			printf("i=%d\n", i);
			if(i == sizeof(locale_real_names)/sizeof(const char *))
				printf("[%s.locale] superfluous entry: %s\n", locale, buf);
		}
	}
	fclose(fd);
	if(buf)
		free(buf);
	char *_mem = (char *) realloc(*mem, memp - *mem);
	if (_mem) {
		if (_mem != *mem) {
			// most likely doesn't happen
			for(unsigned int i = 1; i < sizeof(locale_real_names)/sizeof(const char *); i++)
				if (loadData[i] != locale_real_names[i])
					loadData[i] -= *mem - _mem;
			*mem = _mem;
		}
	}

	for (unsigned j = 1; j < (sizeof(locale_real_names)/sizeof(const char *)); j++)
		if (loadData[j] == locale_real_names[j])
		{
			printf("[%s.locale] missing entry: %s\n", locale, locale_real_names[j]);
			if(!asdefault)
				loadData[j] = defaultData[j];
		}

	return UNICODE_FONT;
}

const char * CLocaleManager::getText(const neutrino_locale_t keyName) const
{
	return localeData[keyName];
}

static const neutrino_locale_t locale_weekday[7] =
{
	LOCALE_DATE_SUN,
	LOCALE_DATE_MON,
	LOCALE_DATE_TUE,
	LOCALE_DATE_WED,
	LOCALE_DATE_THU,
	LOCALE_DATE_FRI,
	LOCALE_DATE_SAT
};

static const neutrino_locale_t locale_month[12] =
{
	LOCALE_DATE_JAN,
	LOCALE_DATE_FEB,
	LOCALE_DATE_MAR,
	LOCALE_DATE_APR,
	LOCALE_DATE_MAY,
	LOCALE_DATE_JUN,
	LOCALE_DATE_JUL,
	LOCALE_DATE_AUG,
	LOCALE_DATE_SEP,
	LOCALE_DATE_OCT,
	LOCALE_DATE_NOV,
	LOCALE_DATE_DEC
};


neutrino_locale_t CLocaleManager::getMonth(const struct tm * struct_tm_p)
{
	return locale_month[struct_tm_p->tm_mon];
}

neutrino_locale_t CLocaleManager::getMonth(const int mon)
{
	if(mon > -1 && mon < 12)
		return locale_month[mon];
	else
		return LOCALE_MESSAGEBOX_ERROR;
}

neutrino_locale_t CLocaleManager::getWeekday(const struct tm * struct_tm_p)
{
	return locale_weekday[struct_tm_p->tm_wday];
}

neutrino_locale_t CLocaleManager::getWeekday(const int wday)
{
	if(wday > -1 && wday < 7)
		return locale_weekday[wday];
	else
		return LOCALE_MESSAGEBOX_ERROR;
}
