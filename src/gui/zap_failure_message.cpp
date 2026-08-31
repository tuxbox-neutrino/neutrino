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

#include <gui/frontend_label.h>

#include <zapit/frontend_c.h>
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

std::string CZapFailureMessageBuilder::formatLocaleText(neutrino_locale_t loc, const std::string &first, const std::string &second) const
{
	std::string text = formatLocaleText(loc, first);
	/* Searching from the start again is safe: the first argument is already
	   substituted, and a tuner label never contains a placeholder itself. */
	size_t pos = text.find("%s");
	if (pos != std::string::npos)
		text.replace(pos, 2, second);
	return text;
}

/* The reception path in the words the tuner setup uses, because that is the
   menu the user is being sent to. Every concrete delivery system falls into
   exactly one of the three groups, and channels read from services.xml carry
   the group mask itself; only UNKNOWN_DS has no home, and then the message is
   better off saying nothing than saying something wrong. */
std::string CZapFailureMessageBuilder::getReceptionName(delivery_system_t delsys) const
{
	if (CFrontend::isSat(delsys))
		return g_Locale->getText(LOCALE_TUNERSETUP_SAT);
	if (CFrontend::isCable(delsys))
		return g_Locale->getText(LOCALE_TUNERSETUP_CABLE);
	if (CFrontend::isTerr(delsys))
		return g_Locale->getText(LOCALE_TUNERSETUP_TERR);

	return "";
}

/* The one sentence that turns a correct diagnosis into something the user can
   act on: a tuner for this channel exists and is merely switched off. It names
   the tuner the way the tuner list does, and quotes the mode the way the mode
   selector does, so both are findable rather than merely described. */
std::string CZapFailureMessageBuilder::buildDisabledTunerText(const CZapFailureInfo &failure_info) const
{
	if (!failure_info.has_disabled_matching_tuner)
		return "";

	const std::string label = getFrontendLabelForDevice(failure_info.disabled_adapter,
			failure_info.disabled_number);
	if (label.empty())
		return "";

	const std::string mode = g_Locale->getText(LOCALE_SATSETUP_FE_MODE_UNUSED);
	const neutrino_locale_t loc = failure_info.disabled_tuner_needs_source
		? LOCALE_INFOVIEWER_NOTAVAILABLE_TUNER_DISABLED_SOURCE
		: LOCALE_INFOVIEWER_NOTAVAILABLE_TUNER_DISABLED;

	return formatLocaleText(loc, label, mode);
}

/* Both configuration reasons end the same way: name the disabled candidate if
   there is one, then point at the tuner setup. Only REASON_NO_SOURCE_CONFIG
   used to do the second half, although these two are the ones that most
   plainly mean "your tuner configuration is wrong". */
std::string CZapFailureMessageBuilder::buildTunerSetupAdvice(const CZapFailureInfo &failure_info) const
{
	std::string advice;

	const std::string disabled = buildDisabledTunerText(failure_info);
	if (!disabled.empty())
	{
		advice += "\n";
		advice += disabled;
	}

	advice += "\n";
	advice += g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_HINT_CHECK_TUNER_SETUP);

	return advice;
}

std::string CZapFailureMessageBuilder::buildReasonText(const CZapFailureInfo &failure_info) const
{
	switch (failure_info.reason)
	{
		case CZapFailureInfo::REASON_STREAM_START_FAILED:
			return g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_STREAM_START_FAILED);
		case CZapFailureInfo::REASON_NO_ACTIVE_TUNER:
		{
			std::string reason = g_Locale->getText(LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_NO_ACTIVE_TUNER);
			reason += buildTunerSetupAdvice(failure_info);
			return reason;
		}
		case CZapFailureInfo::REASON_NO_DELIVERY:
		{
			/* A tuner of the right kind that cannot do this particular
			   standard is not a missing tuner -- saying "no satellite tuner"
			   about a satellite tuner would send the user looking for
			   hardware they already own. */
			std::string reason = g_Locale->getText(failure_info.active_kind_without_delivery
					? LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_NO_STANDARD
					: LOCALE_INFOVIEWER_NOTAVAILABLE_REASON_NO_DELIVERY);

			if (failure_info.has_delsys)
			{
				const std::string reception = getReceptionName(failure_info.delsys);
				if (!reception.empty())
				{
					reason += "\n";
					reason += formatLocaleText(LOCALE_INFOVIEWER_NOTAVAILABLE_NEEDS_RECEPTION, reception);
				}
			}

			reason += buildTunerSetupAdvice(failure_info);
			return reason;
		}
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
