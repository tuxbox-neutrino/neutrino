/*
	Neutrino-GUI

	Copyright (C) 2026

	License: GPL
*/

#ifndef __zap_failure_analyzer__
#define __zap_failure_analyzer__

#include <zapit/types.h>
#include <zapit/frontend_types.h>

struct CZapFailureInfo
{
	enum reason_t
	{
		REASON_NONE = 0,
		REASON_STREAM_START_FAILED,
		REASON_NO_ACTIVE_TUNER,
		REASON_NO_DELIVERY,
		REASON_NO_SOURCE_CONFIG,
		REASON_NO_FREE_TUNER,
		REASON_TUNE_FAILED
	};

	reason_t reason;
	bool has_satellite_position;
	t_satellite_position satellite_position;

	/* What the channel needs, so the message can name it instead of saying
	   "delivery system" and leaving the user to guess. */
	bool has_delsys;
	delivery_system_t delsys;

	/* A frontend that could serve this channel but is switched off. This is
	   the one fact that turns "no tuner supports this" into something the
	   user can act on, and it is only ever set for a frontend the tuner
	   setup would actually let them enable. */
	bool has_disabled_matching_tuner;
	int disabled_adapter;
	int disabled_number;

	/* ...but enabling it is not always enough: a satellite tuner still
	   needs the channel's position configured. Saying "switch it on" alone
	   would then send the user straight into the next failure. */
	bool disabled_tuner_needs_source;

	/* An enabled frontend of the right kind that cannot do this particular
	   standard -- a DVB-S2X transponder on a tuner without multistream, say.
	   "No satellite tuner" would be a lie there; the tuner is a satellite
	   tuner. */
	bool active_kind_without_delivery;

	CZapFailureInfo();
};

class CZapFailureAnalyzer
{
	public:
		static CZapFailureInfo analyze(t_channel_id failed_channel_id, t_channel_id current_channel_id);
};

#endif
