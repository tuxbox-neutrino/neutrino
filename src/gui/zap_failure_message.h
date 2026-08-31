/*
	Neutrino-GUI

	Copyright (C) 2026

	License: GPL
*/

#ifndef __zap_failure_message__
#define __zap_failure_message__

#include <string>

#include <system/locals.h>
#include <system/zap_failure_analyzer.h>

class CZapFailureMessageBuilder
{
	private:
		std::string getSatProviderName(t_satellite_position satpos) const;
		std::string formatLocaleText(neutrino_locale_t loc, const std::string &arg) const;
		std::string formatLocaleText(neutrino_locale_t loc, const std::string &first, const std::string &second) const;
		std::string getReceptionName(delivery_system_t delsys) const;
		std::string buildReceptionText(const CZapFailureInfo &failure_info) const;
		std::string buildDisabledTunerText(const CZapFailureInfo &failure_info) const;
		std::string buildTunerSetupAdvice(const CZapFailureInfo &failure_info) const;

	public:
		std::string buildReasonText(const CZapFailureInfo &failure_info) const;
};

#endif
