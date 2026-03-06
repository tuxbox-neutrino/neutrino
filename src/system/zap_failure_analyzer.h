/*
	Neutrino-GUI

	Copyright (C) 2026

	License: GPL
*/

#ifndef __zap_failure_analyzer__
#define __zap_failure_analyzer__

#include <zapit/types.h>

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

	CZapFailureInfo();
};

class CZapFailureAnalyzer
{
	public:
		static CZapFailureInfo analyze(t_channel_id failed_channel_id, t_channel_id current_channel_id);
};

#endif
