/*
 * $Header: /cvs/tuxbox/apps/dvb/zapit/include/zapit/client/msgtypes.h,v 1.25 2004/10/27 16:08:40 lucgas Exp $
 *
 * types used for clientlib <-> zapit communication - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __msgtypes_h__
#define __msgtypes_h__


#include "zapittypes.h"
#include <connection/basicmessage.h>
#include "zapitclient.h"


#define ZAPIT_UDS_NAME "/tmp/zapit.sock"
#define RESPONSE_GET_BOUQUETS_END_MARKER 0xFFFFFFFF
#define SATNAMES_END_MARKER 0xFFFFFFFF


class CZapitMessages
{
 public:
	static const char ACTVERSION = 7;

	enum commands
		{
			CMD_SHUTDOWN		           =  1,

			CMD_REGISTEREVENTS                 =  2,
			CMD_UNREGISTEREVENTS               =  3,

			CMD_ZAPTO                          =  4,
			CMD_ZAPTO_CHANNELNR                =  5,
			CMD_ZAPTO_SERVICEID                =  6,
			CMD_ZAPTO_SUBSERVICEID             =  7,
			CMD_ZAPTO_SERVICEID_NOWAIT         =  8,
			CMD_ZAPTO_SUBSERVICEID_NOWAIT      =  9,

			CMD_STOP_VIDEO                     = 10,		// not supported yet
			CMD_SET_MODE                       = 11,
			CMD_GET_MODE                       = 12,
			CMD_GET_LAST_CHANNEL               = 13,
			CMD_GET_APID_VPID                  = 14,		// not supported yet
			CMD_GET_VTXT_PID                   = 15,		// not supported yet
			CMD_GET_NVOD_CHANNELS              = 16,		// not supported yet
			CMD_REINIT_CHANNELS                = 17,
			CMD_GET_CHANNELS                   = 18,
			CMD_GET_BOUQUETS                   = 19,
			CMD_GET_BOUQUET_CHANNELS           = 20,
			CMD_GET_CA_INFO                    = 21,		// not supported yet
			CMD_GET_CURRENT_SERVICEID          = 22,
			CMD_GET_CURRENT_SERVICEINFO        = 23,
			CMD_GET_DELIVERY_SYSTEM            = 24,
			CMD_GET_CURRENT_SATELLITE_POSITION = 25,
			CMD_GET_CURRENT_TP                 = 26,

			CMD_SCANSTART                      = 27,
			CMD_SCAN_TP                        = 28,
			CMD_SCANREADY                      = 29,
			CMD_SCANGETSATLIST                 = 30,
			CMD_SCANSETSCANSATLIST             = 31,
			CMD_SCANSETSCANMOTORPOSLIST        = 32,
			CMD_SCANSETDISEQCTYPE              = 33,
			CMD_SCANSETDISEQCREPEAT            = 34,
			CMD_SCANSETBOUQUETMODE             = 35,

			CMD_BQ_ADD_BOUQUET                 = 36,
			CMD_BQ_MOVE_BOUQUET                = 37,
			CMD_BQ_MOVE_CHANNEL                = 38,
			CMD_BQ_DELETE_BOUQUET              = 39,
			CMD_BQ_RENAME_BOUQUET              = 40,
			CMD_BQ_EXISTS_BOUQUET              = 41,
			CMD_BQ_SET_LOCKSTATE               = 42,
			CMD_BQ_SET_HIDDENSTATE             = 43,
			CMD_BQ_ADD_CHANNEL_TO_BOUQUET      = 44,
			CMD_BQ_EXISTS_CHANNEL_IN_BOUQUET   = 45,
			CMD_BQ_REMOVE_CHANNEL_FROM_BOUQUET = 46,
			CMD_BQ_RENUM_CHANNELLIST           = 47,
			CMD_BQ_RESTORE                     = 48,
			/* unused:			CMD_BQ_COMMIT_CHANGE               = 49, */
			CMD_BQ_SAVE_BOUQUETS               = 50,

			CMD_SET_RECORD_MODE                = 51,
			CMD_GET_RECORD_MODE                = 52,
			CMD_SB_START_PLAYBACK              = 53,
			CMD_SB_STOP_PLAYBACK               = 54,
			CMD_SB_GET_PLAYBACK_ACTIVE         = 55,
			CMD_SET_DISPLAY_FORMAT             = 56,
			CMD_SET_AUDIO_MODE                 = 57,
			CMD_READY                          = 58,
			CMD_GETPIDS                        = 59,
			CMD_SETSUBSERVICES                 = 60,
			CMD_SET_AUDIOCHAN                  = 61,
			CMD_MUTE                           = 62,
			CMD_SET_VOLUME                     = 63,
			CMD_SET_STANDBY                    = 64,
			CMD_SET_VIDEO_SYSTEM               = 65,
			CMD_SET_NTSC                       = 66,

			CMD_NVOD_SUBSERVICE_NUM            = 67,
			CMD_SEND_MOTOR_COMMAND             = 68,

			CMD_GET_CHANNEL_NAME               = 69,
			CMD_IS_TV_CHANNEL                  = 70,

			CMD_GET_FE_SIGNAL                  = 71,

			CMD_SET_AE_IEC_ON                  = 73,
			CMD_SET_AE_IEC_OFF                 = 74,
			CMD_GET_AE_IEC_STATE               = 75,
			CMD_SET_AE_PLAYBACK_SPTS           = 76,
			CMD_SET_AE_PLAYBACK_PES            = 77,
			CMD_GET_AE_PLAYBACK_STATE          = 78,

			CMD_SCANSETTYPE                    = 79,
			CMD_RELOAD_CURRENTSERVICES	   = 80,

			CMD_TUNE_TP                        = 91,
			CMD_SB_LOCK_PLAYBACK		   = 92,
			CMD_SB_UNLOCK_PLAYBACK		   = 93,
			CMD_GET_BOUQUET_NCHANNELS          = 94,
			CMD_SET_EVENT_MODE                 = 95,
			CMD_REZAP		  	   = 96,
			CMD_GETCONFIG		  	   = 97,
			CMD_SETCONFIG		  	   = 98,
			CMD_SCANSTOP			   = 99,
			CMD_GET_VOLUME			   = 104,
			CMD_GET_AUDIO_MODE		   = 105,
			CMD_GET_MUTE_STATUS		   = 106,
			CMD_GET_ASPECTRATIO		   = 107,
			CMD_SET_ASPECTRATIO		   = 108,
			CMD_GET_MODE43			   = 109,
			CMD_SET_MODE43			   = 110,
			CMD_STOP_PIP			   = 111

		};

	struct commandBoolean
	{
		bool truefalse;
	};

	struct commandInt
	{
		int val;
	};

	struct commandVolume
	{
		unsigned int left;
		unsigned int right;
	};

	struct commandSetRecordMode
	{
		bool activate;
	};

	struct commandZapto
	{
		unsigned int bouquet;
		unsigned int channel;
	};

	struct commandZaptoChannelNr
	{
		unsigned int channel;
	};

	struct commandZaptoServiceID
	{
		t_channel_id channel_id;
		bool record;
		bool pip;
		bool epg;
	};

	struct commandSetAudioChannel
	{
		unsigned int channel;
	};

	struct commandGetBouquets
	{
		bool emptyBouquetsToo;
		CZapitClient::channelsMode mode;
	};

	struct commandSetMode
	{
		CZapitClient::channelsMode mode;
	};

	struct commandGetBouquetChannels
	{
		unsigned int               bouquet;
		CZapitClient::channelsMode mode;
	};

	struct commandGetChannels
	{
		CZapitClient::channelsMode  mode;
		CZapitClient::channelsOrder order;
	};

	struct commandExistsChannelInBouquet
	{
		unsigned int bouquet;
		t_channel_id channel_id;
	};


	struct commandAddChannelToBouquet
	{
		unsigned int bouquet;
		t_channel_id channel_id;
	};

	struct commandRemoveChannelFromBouquet
	{
		unsigned int bouquet;
		t_channel_id channel_id;
	};

	struct commandDeleteBouquet
	{
		unsigned int bouquet;
	};

	struct commandRenameBouquet
	{
		unsigned int bouquet;
	};

	struct commandMoveBouquet
	{
		unsigned int bouquet;
		unsigned int newPos;
	};

	struct commandStartScan
	{
		unsigned int satelliteMask;
	};

	struct commandBouquetState
	{
		unsigned int bouquet;
		bool	     state;
	};

	struct commandMoveChannel
	{
		unsigned int               bouquet;
		unsigned int               oldPos;
		unsigned int               newPos;
		CZapitClient::channelsMode mode;
	};




	struct responseGeneralTrueFalse
	{
		bool status;
	};

	struct responseGeneralInteger
	{
		int number;
	};

	struct responseGetChannelName
	{
		char name[CHANNEL_NAME_SIZE];
	};

	struct responseGetRecordModeState
	{
		bool activated;
	};

	struct responseGetMode
	{
		CZapitClient::channelsMode  mode;
	};

	struct responseGetPlaybackState
	{
		bool activated;
	};

	struct responseGetCurrentServiceID
	{
		t_channel_id channel_id;
	};

	struct responseZapComplete
	{
		unsigned int zapStatus;
	};

	struct responseCmd
	{
		unsigned char cmd;
	};

	struct responseIsScanReady
	{
		bool scanReady;
		unsigned int satellite;
		unsigned int processed_transponder;
		unsigned int transponder;
		unsigned int services;
	};

	struct responseDeliverySystem
	{
		delivery_system_t system;
	};

	struct commandMotor
	{
		uint8_t cmdtype;
		uint8_t cmd;
		uint8_t address;
		uint8_t num_parameters;
		uint8_t param1;
		uint8_t param2;
	};


};


#endif /* __msgtypes_h__ */
