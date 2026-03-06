/*
	Neutrino-GUI

	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "zap_failure_message.h"

#include <cstdio>
#include <cstdlib>

#include <global.h>

#include <zapit/satconfig.h>
#include <zapit/zapit.h>

std::string CZapFailureMessageBuilder::getSatProviderName(t_satellite_position satpos) const
{
	std::string source = CServiceManager::getInstance()->GetSatelliteName(satpos);
	if (!source.empty())
		return source;

	if (SAT_POSITION_CABLE(satpos))
		return g_Locale->getText(LOCALE_CABLESETUP_PROVIDER);
	if (SAT_POSITION_TERR(satpos))
		return g_Locale->getText(LOCALE_TERRESTRIALSETUP_PROVIDER);

	char pos[32];
	snprintf(pos, sizeof(pos), "%c%03d.%d", satpos < 0 ? 'W' : 'E', abs((int)satpos) / 10, abs((int)satpos) % 10);
	return pos;
}

std::string CZapFailureMessageBuilder::formatLocaleText(neutrino_locale_t loc, const std::string &arg) const
{
	std::string text = g_Locale->getText(loc);
	size_t pos = text.find("%s");
	if (pos != std::string::npos)
		text.replace(pos, 2, arg);
	return text;
}

std::string CZapFailureMessageBuilder::buildReasonText(const CZapFailureInfo &failure_info) const
{
	switch (failure_info.reason)
	{
		case CZapFailureInfo::REASON_STREAM_START_FAILED:
			return g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_STREAM_START_FAILED);
		case CZapFailureInfo::REASON_NO_ACTIVE_TUNER:
			return g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_NO_ACTIVE_TUNER);
		case CZapFailureInfo::REASON_NO_DELIVERY:
			return g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_NO_DELIVERY);
		case CZapFailureInfo::REASON_NO_SOURCE_CONFIG:
		{
			std::string reason = g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_NO_SOURCE_CONFIG);
			if (failure_info.has_satellite_position)
			{
				reason += "\n";
				reason += formatLocaleText(LOCALE_INFOVIEWER_NOTAVAILABLE_SOURCE, getSatProviderName(failure_info.satellite_position));
			}
			reason += "\n";
			reason += g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_HINT_CHECK_TUNER_SETUP);
			return reason;
		}
		case CZapFailureInfo::REASON_NO_FREE_TUNER:
			return g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_NO_FREE_TUNER);
		case CZapFailureInfo::REASON_TUNE_FAILED:
		{
			std::string reason = g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_TUNE_FAILED);
			if (failure_info.has_satellite_position)
			{
				reason += "\n";
				reason += formatLocaleText(LOCALE_INFOVIEWER_NOTAVAILABLE_SOURCE, getSatProviderName(failure_info.satellite_position));
			}
			return reason;
		}
		case CZapFailureInfo::REASON_NONE:
		default:
			return "";
	}
}
