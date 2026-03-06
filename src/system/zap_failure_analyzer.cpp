/*
	Neutrino-GUI

	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "zap_failure_analyzer.h"

#include <zapit/channel.h>
#include <zapit/femanager.h>
#include <zapit/satconfig.h>
#include <zapit/zapit.h>

CZapFailureInfo::CZapFailureInfo()
	: reason(REASON_NONE)
	, has_satellite_position(false)
	, satellite_position(0)
{
}

CZapFailureInfo CZapFailureAnalyzer::analyze(t_channel_id failed_channel_id, t_channel_id current_channel_id)
{
	CZapFailureInfo info;

	CZapitChannel *channel = NULL;
	if (failed_channel_id)
		channel = CServiceManager::getInstance()->FindChannel(failed_channel_id);
	if (!channel && current_channel_id)
		channel = CServiceManager::getInstance()->FindChannel(current_channel_id);
	if (!channel)
		return info;

	if (IS_WEBCHAN(channel->getChannelID()))
	{
		info.reason = CZapFailureInfo::REASON_STREAM_START_FAILED;
		return info;
	}

	info.has_satellite_position = true;
	info.satellite_position = channel->getSatellitePosition();

	bool have_active_fe = false;
	bool have_matching_delivery = false;
	bool have_configured_source = false;
	bool have_unlocked_configured = false;
	const int frontend_count = CFEManager::getInstance()->getFrontendCount();

	for (int i = 0; i < frontend_count; i++)
	{
		CFrontend *fe = CFEManager::getInstance()->getFE(i);
		if (!fe)
			continue;

		if (fe->getMode() == CFrontend::FE_MODE_UNUSED || CFrontend::linked(fe->getMode()))
			continue;

		have_active_fe = true;

		if (!fe->supportsDelivery(channel->delsys))
			continue;

		have_matching_delivery = true;

		satellite_map_t &satmap = fe->getSatellites();
		sat_iterator_t sit = satmap.find(channel->getSatellitePosition());
		if (sit == satmap.end() || !sit->second.configured)
			continue;

		have_configured_source = true;
		if (!fe->Locked())
			have_unlocked_configured = true;
	}

	const bool can_tune_now = CFEManager::getInstance()->canTune(channel);

	if (!have_active_fe)
		info.reason = CZapFailureInfo::REASON_NO_ACTIVE_TUNER;
	else if (!have_matching_delivery)
		info.reason = CZapFailureInfo::REASON_NO_DELIVERY;
	else if (!have_configured_source)
		info.reason = CZapFailureInfo::REASON_NO_SOURCE_CONFIG;
	else if (!can_tune_now && !have_unlocked_configured)
		info.reason = CZapFailureInfo::REASON_NO_FREE_TUNER;
	else
		info.reason = CZapFailureInfo::REASON_TUNE_FAILED;

	return info;
}
