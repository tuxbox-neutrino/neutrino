/*
	Neutrino-GUI

	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "frontend_label.h"

#include <cstdio>

#include <global.h>

#include <zapit/femanager.h>
#include <zapit/frontend_c.h>

/* The letter is decoration, but it is decoration the user reads off the list
   entry, so it has to come from one place. 24 covers every configuration this
   runs on; beyond that the label drops the letter rather than reading past the
   array. */
static const char *tuner_desc[24] =
{
	"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
	"M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X"
};

const char *getFrontendLetter(int index)
{
	const int desc_count = (int)(sizeof(tuner_desc) / sizeof(tuner_desc[0]));
	return (index >= 0 && index < desc_count) ? tuner_desc[index] : "";
}

std::string getFrontendLabel(int index)
{
	CFrontend *fe = CFEManager::getInstance()->getFE(index);
	if (fe == NULL)
		return "";

	const char *desc = getFrontendLetter(index);

	const char *kind =
		fe->isHybrid() ? g_Locale->getText(LOCALE_SCANTS_ACTHYBRID)
		: fe->hasSat()   ? g_Locale->getText(LOCALE_SCANTS_ACTSATELLITE)
		: fe->hasTerr()  ? g_Locale->getText(LOCALE_SCANTS_ACTTERRESTRIAL)
		: g_Locale->getText(LOCALE_SCANTS_ACTCABLE);

	char name[255];
	snprintf(name, sizeof(name), "%s %02d: [%s] %s %s",
		g_Locale->getText(LOCALE_SATSETUP_FE_TUNER), index + 1,
		desc, kind, fe->getName());

	return name;
}

std::string getFrontendLabelForDevice(int adapter, int number)
{
	/* The list numbers by the manager's index, not by adapter/frontend, and
	   the two diverge as soon as one frontend is busy or absent. So the
	   coordinates are resolved back to that index instead of computed from
	   them. */
	const int count = CFEManager::getInstance()->getFrontendCount();
	for (int i = 0; i < count; i++)
	{
		CFrontend *fe = CFEManager::getInstance()->getFE(i);
		if (fe && fe->getAdapter() == adapter && fe->getNumber() == number)
			return getFrontendLabel(i);
	}

	return "";
}

std::string getBusyFrontendName(int adapter, int number)
{
	char name[255];
	const int display_index = adapter * MAX_FE + number;
	const char *desc = getFrontendLetter(display_index);

	if (desc[0] != '\0')
	{
		snprintf(name, sizeof(name), "%s %02d: [%s] adapter%d/frontend%d",
			g_Locale->getText(LOCALE_SATSETUP_FE_TUNER),
			display_index + 1, desc, adapter, number);
	}
	else
	{
		snprintf(name, sizeof(name), "%s adapter%d/frontend%d",
			g_Locale->getText(LOCALE_SATSETUP_FE_TUNER), adapter, number);
	}

	return name;
}
