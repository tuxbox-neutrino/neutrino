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

//static const char * iso639filename = "/usr/share/iso-codes/iso-639.tab";
static const char * iso639filename = "/share/iso-codes/iso-639.tab";

#if 1
#include <stdlib.h>
#include <stdio.h>

#define ISO639_TABLE_SIZE 489
typedef struct
{
	char * iso_639_2_code;
	char * name;
} iso639_t;

iso639_t iso639[ISO639_TABLE_SIZE];

int mycompare(const void * a, const void * b)
{
	return strcmp(((iso639_t *)a)->iso_639_2_code, ((iso639_t *)b)->iso_639_2_code);
}

void initialize_iso639_map(void)
{
	unsigned i = 0;
	std::string s, t, v;
	std::ifstream in(iso639filename);
	if (in.is_open())
	{
		while (in.peek() == '#')
			getline(in, s);
		while (in >> s >> t >> v >> std::ws)
		{
			getline(in, v);

			if (i == ISO639_TABLE_SIZE)
			{
				printf("ISO639 table overflow\n");
				goto do_sorting;
			}

			iso639[i].iso_639_2_code = strdup(s.c_str());
			iso639[i].name           = strdup(v.c_str());

			i++;

			if (s != t)
			{
				if (i == ISO639_TABLE_SIZE)
				{
					printf("ISO639 table overflow\n");
					goto do_sorting;
				}

				iso639[i].iso_639_2_code = strdup(t.c_str());
//				iso639[i].name           = strdup(v.c_str());
				iso639[i].name           = iso639[i - 1].name;

				i++;
			}
		}
		if (i != ISO639_TABLE_SIZE)
		{
			printf("ISO639 table underflow\n");
			while(i < ISO639_TABLE_SIZE)
			{
				iso639[i].iso_639_2_code = iso639[i].name = (char *)iso639filename; // fill with junk
				i++;
			}
		}
	do_sorting:
		qsort(iso639, ISO639_TABLE_SIZE, sizeof(iso639_t), mycompare);
	}
	else
		printf("Loading %s failed.\n", iso639filename);
}

const char * getISO639Description(const char * const iso)
{
	iso639_t tmp;
	tmp.iso_639_2_code = (char *)iso;

	void * value = bsearch(&tmp, iso639, ISO639_TABLE_SIZE, sizeof(iso639_t), mycompare);
	if (value == NULL)
		return iso;
	else
		return ((iso639_t *)value)->name;
}
#else
#include <iostream>
#include <map>

static std::map<std::string, std::string> iso639;

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
			if (s != t)
				iso639[t] = v;
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
#endif

CLocaleManager::CLocaleManager()
{
	localeData = new char * [sizeof(locale_real_names)/sizeof(const char *)];
	for (unsigned int i = 0; i < (sizeof(locale_real_names)/sizeof(const char *)); i++)
		localeData[i] = (char *)locale_real_names[i];
}

CLocaleManager::~CLocaleManager()
{
	for (unsigned j = 0; j < (sizeof(locale_real_names)/sizeof(const char *)); j++)
		if (localeData[j] != locale_real_names[j])
			free(localeData[j]);

	delete localeData;
}

const char * path[2] = {"/var/tuxbox/config/locale/", DATADIR "/neutrino/locale/"};

CLocaleManager::loadLocale_ret_t CLocaleManager::loadLocale(const char * const locale)
{
	unsigned int i;
	FILE * fd;

	initialize_iso639_map();

	for (i = 0; i < 2; i++)
	{
		std::string filename = path[i];
		filename += locale;
		filename += ".locale";
		
		fd = fopen(filename.c_str(), "r");
		if (fd)
			break;
	}
	
	if (i == 2)
	{		
		perror("cannot read locale");
		return NO_SUCH_LOCALE;
	}

	for (unsigned j = 0; j < (sizeof(locale_real_names)/sizeof(const char *)); j++)
		if (localeData[j] != locale_real_names[j])
		{
			free(localeData[j]);
			localeData[j] = (char *)locale_real_names[j];
		}

	char buf[1000];

	i = 1;

	while(!feof(fd))
	{
		if(fgets(buf,sizeof(buf),fd)!=NULL)
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

			for(i = 1; i < sizeof(locale_real_names)/sizeof(const char *); i++)
			{
//printf("[%s] [%s]\n", buf,locale_real_names[i]);
				if(!strcmp(buf,locale_real_names[i]))
				{
					if(localeData[i] == locale_real_names[i])
						localeData[i] = strdup(text.c_str());
					else
						printf("[%s.locale] dup entry: %s\n", locale, locale_real_names[i]);
					break;
				}
			}
//			printf("i=%d\n", i);
			if(i == sizeof(locale_real_names)/sizeof(const char *))
				printf("[%s.locale] superfluous entry: %s\n", locale, buf);
#if 0

			while (1)
			{
				j = (i >= (sizeof(locale_real_names)/sizeof(const char *))) ? -1 : strcmp(buf, locale_real_names[i]);
				if (j > 0)
				{
					printf("[%s.locale] missing entry:     %s\n", locale, locale_real_names[i]);
					i++;
				}
				else
					break;
			}
			if (j == 0)
			{
				localeData[i] = strdup(text.c_str());
				i++;
			}
			else
			{
				printf("[%s.locale] superfluous entry: %s\n", locale, buf);
			}
#endif
		}
	}
	fclose(fd);
	for (unsigned j = 1; j < (sizeof(locale_real_names)/sizeof(const char *)); j++)
		if (localeData[j] == locale_real_names[j])
		{
			printf("[%s.locale] missing entry: %s\n", locale, locale_real_names[j]);
		}

	return (
		(strcmp(locale, "bosanski") == 0) ||
		(strcmp(locale, "ellinika") == 0) ||
		(strcmp(locale, "russkij") == 0) ||
		(strcmp(locale, "utf8") == 0)
		/* utf8.locale is a generic name that can be used for new locales which need characters outside the ISO-8859-1 character set */
		) ? UNICODE_FONT : ISO_8859_1_FONT;
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

neutrino_locale_t CLocaleManager::getWeekday(const struct tm * struct_tm_p)
{
	return locale_weekday[struct_tm_p->tm_wday];
}

