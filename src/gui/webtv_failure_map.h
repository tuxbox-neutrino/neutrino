/*
 * webtv_failure_map -- what the app calls a transport failure that the
 * shared stream input core (libstb-hal, streaminput) has classified.
 *
 * The app's webtv_error_reason_t is frozen and drives the restart
 * decision (CMoviePlayerGui::prepareWebtvRestartLocked), so this table
 * must reproduce the historic if-chain bit for bit. It is header-only
 * and needs nothing from neutrino -- only libstb-hal's streaminput
 * bridge (and thus libavutil) -- so the build repo's shell suite can
 * include it and pin every row against the LIVE core. That is the guard
 * against a remapping in the other repository (say, a new AVERROR
 * feeding STREAM_FAILURE_ABORTED) silently widening the restart set.
 *
 * (C) 2026 Thilo Graf
 *
 * License: GPLv2 or later
 */
#ifndef __WEBTV_FAILURE_MAP_H__
#define __WEBTV_FAILURE_MAP_H__

#include <streaminput_ffmpeg.h>

/* Verdicts the table can reach. FALLTHROUGH means "the app has no word
 * for this class": the caller applies its DNS-qualified connect
 * failure, exactly as the old chain did for every code it did not list.
 * Only the other four verdicts are ever restarted. */
typedef enum
{
	WEBTV_MAP_FALLTHROUGH = 0,
	WEBTV_MAP_RESET_BY_PEER,
	WEBTV_MAP_HTTP_SERVER_ERROR,
	WEBTV_MAP_IMMEDIATE_EXIT,
	WEBTV_MAP_INVALID_DATA
} webtv_map_verdict_t;

static inline webtv_map_verdict_t webtv_map_core_failure(int averror)
{
	switch (streaminput_classify_averror(averror))
	{
		case STREAM_FAILURE_HTTP_5XX:
			return WEBTV_MAP_HTTP_SERVER_ERROR;
		case STREAM_FAILURE_INVALID_MANIFEST:
			return WEBTV_MAP_INVALID_DATA;
		case STREAM_FAILURE_ABORTED:
			return WEBTV_MAP_IMMEDIATE_EXIT;
		case STREAM_FAILURE_TEMPORARY_NETWORK:
			/* ECONNRESET and EIO share this class; only the reset was
			 * ever reported -- and restarted -- as a peer reset. */
			if (averror == AVERROR(ECONNRESET))
				return WEBTV_MAP_RESET_BY_PEER;
			return WEBTV_MAP_FALLTHROUGH;
		case STREAM_FAILURE_NONE:
		case STREAM_FAILURE_CONNECTION_FAILED:
		case STREAM_FAILURE_CONNECTION_TIMEOUT:
		case STREAM_FAILURE_HTTP_4XX:
		case STREAM_FAILURE_UNSUPPORTED_PROTOCOL:
		case STREAM_FAILURE_END_OF_STREAM:
		case STREAM_FAILURE_UNKNOWN:
			return WEBTV_MAP_FALLTHROUGH;
	}
	/* No default label: a new stream_failure_class_t trips -Wswitch here
	 * and gets a decision instead of a silent fall-through. */
	return WEBTV_MAP_FALLTHROUGH;
}

/* Stable lowercase token per verdict, for the test and the log. */
static inline const char *webtv_map_verdict_name(webtv_map_verdict_t verdict)
{
	switch (verdict)
	{
		case WEBTV_MAP_FALLTHROUGH:
			return "fallthrough";
		case WEBTV_MAP_RESET_BY_PEER:
			return "reset-by-peer";
		case WEBTV_MAP_HTTP_SERVER_ERROR:
			return "http-server-error";
		case WEBTV_MAP_IMMEDIATE_EXIT:
			return "immediate-exit";
		case WEBTV_MAP_INVALID_DATA:
			return "invalid-data";
	}
	return "fallthrough";
}

#endif /* __WEBTV_FAILURE_MAP_H__ */
