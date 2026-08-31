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
	, has_delsys(false)
	, delsys(UNKNOWN_DS)
	, has_disabled_matching_tuner(false)
	, disabled_adapter(0)
	, disabled_number(0)
	, disabled_tuner_needs_source(false)
	, active_kind_without_delivery(false)
{
}

/* Whether a frontend could carry this channel at all. This has to answer the
   same way CFEManager::getFrontend() does, or the message promises a fix that
   the tuner would refuse anyway: a hybrid frontend the user pinned to another
   reception path is skipped there too, and supportsDelivery() alone does not
   know about that pin. */
static bool frontendServesDelivery(CFrontend *fe, delivery_system_t delsys)
{
	if (!fe->supportsDelivery(delsys))
		return false;

	if (fe->isHybrid() && fe->forcedDelivery(delsys))
		return false;

	return true;
}

/* Whether the channel's source is configured on this frontend. Mirrors the
   satellite check in CFEManager::getFrontend(); cable and terrestrial carry no
   position to configure. */
static bool frontendHasSource(CFrontend *fe, t_satellite_position position)
{
	if (!fe->hasSat() || SAT_POSITION_CABLE(position) || SAT_POSITION_TERR(position))
		return true;

	satellite_map_t &satmap = fe->getSatellites();
	sat_iterator_t sit = satmap.find(position);

	return (sit != satmap.end()) && sit->second.configured;
}

/* Whether the frontend is the kind of tuner this channel calls for, regardless
   of the individual standard. */
static bool frontendIsRightKind(CFrontend *fe, delivery_system_t delsys)
{
	if (CFrontend::isSat(delsys))
		return fe->hasSat();
	if (CFrontend::isCable(delsys))
		return fe->hasCable();
	if (CFrontend::isTerr(delsys))
		return fe->hasTerr();

	return false;
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

	const t_satellite_position position = channel->getSatellitePosition();

	info.has_satellite_position = true;
	info.satellite_position = position;
	info.has_delsys = true;
	info.delsys = channel->delsys;

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

		if (fe->getMode() == CFrontend::FE_MODE_UNUSED)
		{
			/* Remember the first one that would do, so the message can name
			   it. deliverySystemMask survives the Close() that linkFrontends()
			   does to unused frontends -- it is filled in CFEManager::Init(),
			   before any mode is known, and nothing clears it. */
			if (!info.has_disabled_matching_tuner && frontendServesDelivery(fe, channel->delsys))
			{
				info.has_disabled_matching_tuner = true;
				info.disabled_adapter = fe->getAdapter();
				info.disabled_number = fe->getNumber();
				info.disabled_tuner_needs_source = !frontendHasSource(fe, position);
			}
			continue;
		}

		/* A linked frontend is bound to its master and is not something the
		   user can enable on its own, so it is no candidate either way. */
		if (CFrontend::linked(fe->getMode()))
			continue;

		have_active_fe = true;

		if (!frontendServesDelivery(fe, channel->delsys))
		{
			if (frontendIsRightKind(fe, channel->delsys))
				info.active_kind_without_delivery = true;
			continue;
		}

		have_matching_delivery = true;

		if (!frontendHasSource(fe, position))
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
