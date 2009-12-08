/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __controldMsg__
#define __controldMsg__

#include <controldclient/controldtypes.h>
#include <connection/basicmessage.h>

#define CONTROLD_UDS_NAME "/tmp/controld.sock"


class CControldMsg : public CBasicMessage
{

	public:

		static const CBasicMessage::t_version ACTVERSION = 2;

		enum commands
		{
			CMD_SHUTDOWN = 1,
			CMD_SAVECONFIG,

			CMD_SETVOLUME,
			CMD_GETVOLUME,

			CMD_SETVOLUME_AVS,
			CMD_GETVOLUME_AVS,

			CMD_SETMUTE,
			CMD_GETMUTESTATUS,

			CMD_SETVIDEOFORMAT,
			CMD_GETVIDEOFORMAT,

			CMD_SETVIDEOOUTPUT,
			CMD_GETVIDEOOUTPUT,

			CMD_SETVCROUTPUT,
			CMD_GETVCROUTPUT,

			CMD_SETBOXTYPE,
			CMD_GETBOXTYPE,

			CMD_SETSCARTMODE,
			CMD_GETSCARTMODE,

			CMD_GETASPECTRATIO,

			CMD_SETVIDEOPOWERDOWN,
			CMD_GETVIDEOPOWERDOWN,

			CMD_REGISTEREVENT,
			CMD_UNREGISTEREVENT,

			CMD_EVENT,

			CMD_SETCSYNC,
			CMD_GETCSYNC,
                        CMD_SETVIDEOMODE,
                        CMD_GETVIDEOMODE

		};
		
		struct commandVolume
		{
			unsigned char volume;
			CControld::volume_type type;
		};

		struct commandMute
		{
			bool mute;
			CControld::volume_type type;
		};

		struct commandVideoFormat
		{
			unsigned char format;
		};

		struct commandVideoOutput
		{
			unsigned char output;
		};

		struct commandVCROutput
		{
			unsigned char vcr_output;
		};

		struct commandBoxType
		{
			CControld::tuxbox_maker_t boxtype;
		};

		struct commandScartMode
		{
			unsigned char mode;
		};

		struct commandVideoPowerSave
		{
			bool powerdown;
		};

		//response structures

		struct responseVideoFormat
		{
			unsigned char format;
		};

		struct responseAspectRatio
		{
			unsigned char aspectRatio;
		};

		struct responseVideoOutput
		{
			unsigned char output;
		};

		struct responseVCROutput
		{
			unsigned char vcr_output;
		};

		struct responseBoxType
		{
			CControld::tuxbox_maker_t boxtype;
		};

		struct responseScartMode
		{
			unsigned char mode;
		};

		struct commandCsync
		{
			unsigned char csync;
		};

		struct responseVideoPowerSave
		{
			bool videoPowerSave;
		};

};

#endif
