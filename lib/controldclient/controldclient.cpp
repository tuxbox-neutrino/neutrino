/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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

#include <string.h>

#include <eventserver.h>
#include <controldclient/controldclient.h>


const unsigned char   CControldClient::getVersion   () const
{
	return CControldMsg::ACTVERSION;
}

const          char * CControldClient::getSocketName() const
{
	return CONTROLD_UDS_NAME;
}

void  CControldClient::shutdown()
{
	send(CControldMsg::CMD_SHUTDOWN);
	close_connection();
}

void CControldClient::setBoxType(CControld::tuxbox_maker_t type)
{
	CControldMsg::commandBoxType msg2;

	msg2.boxtype = type;

	send(CControldMsg::CMD_SETBOXTYPE, (char*)&msg2, sizeof(msg2));

	close_connection();
}

CControld::tuxbox_maker_t CControldClient::getBoxType()
{
	CControldMsg::responseBoxType rmsg;

	send(CControldMsg::CMD_GETBOXTYPE);

	if (!receive_data((char*)&rmsg, sizeof(rmsg)))
		rmsg.boxtype = CControld::TUXBOX_MAKER_UNKNOWN;

	close_connection();

	return rmsg.boxtype;
}

void CControldClient::setScartMode(bool mode)
{
	CControldMsg::commandScartMode msg2;

	msg2.mode = mode;

	send(CControldMsg::CMD_SETSCARTMODE, (char*)&msg2, sizeof(msg2));

	close_connection();
}

char CControldClient::getScartMode()
{
	CControldMsg::responseScartMode rmsg;
	send(CControldMsg::CMD_GETSCARTMODE);
	receive_data((char*)&rmsg, sizeof(rmsg));
	close_connection();

	return rmsg.mode;
}

void CControldClient::setVolume(const char volume, const CControld::volume_type volume_type)
{
	CControldMsg::commandVolume msg2;
	msg2.type = volume_type;
	msg2.volume = volume;
	send(CControldMsg::CMD_SETVOLUME, (char*)&msg2, sizeof(msg2));
	close_connection();
}

char CControldClient::getVolume(const CControld::volume_type volume_type)
{
	CControldMsg::commandVolume rmsg;
	rmsg.type = volume_type;
	send(CControldMsg::CMD_GETVOLUME, (char*)&rmsg, sizeof(rmsg));
	receive_data((char*)&rmsg, sizeof(rmsg));
	close_connection();

	return rmsg.volume;
}

void CControldClient::setVideoFormat(char format)
{
	CControldMsg::commandVideoFormat msg2;

	msg2.format = format;

	send(CControldMsg::CMD_SETVIDEOFORMAT, (char*)&msg2, sizeof(msg2));

	close_connection();
}

char CControldClient::getAspectRatio()
{
	CControldMsg::responseAspectRatio rmsg;

	send(CControldMsg::CMD_GETASPECTRATIO);

	receive_data((char*)&rmsg, sizeof(rmsg));

	close_connection();

	return rmsg.aspectRatio;
}

char CControldClient::getVideoFormat()
{
	CControldMsg::responseVideoFormat rmsg;

	send(CControldMsg::CMD_GETVIDEOFORMAT);

	bool success = receive_data((char*)&rmsg, sizeof(rmsg));

	close_connection();

	return success ? rmsg.format : 2; /* default value is 2 (cf. controld.cpp) */
}

void CControldClient::setVideoOutput(char output)
{
	CControldMsg::commandVideoOutput msg2;

	msg2.output = output;

	send(CControldMsg::CMD_SETVIDEOOUTPUT, (char*)&msg2, sizeof(msg2));

	close_connection();
}

void CControldClient::setVCROutput(char output)
{
	CControldMsg::commandVCROutput msg2;

	msg2.vcr_output = output;

	send(CControldMsg::CMD_SETVCROUTPUT, (char*)&msg2, sizeof(msg2));

	close_connection();
}

char CControldClient::getVideoOutput()
{
	CControldMsg::responseVideoOutput rmsg;

	send(CControldMsg::CMD_GETVIDEOOUTPUT);

	bool success = receive_data((char*)&rmsg, sizeof(rmsg));

	close_connection();

	return success ? rmsg.output : 1; /* default value is 1 (cf. controld.cpp) */
}

char CControldClient::getVCROutput()
{
	CControldMsg::responseVCROutput rmsg;

	send(CControldMsg::CMD_GETVCROUTPUT);

	bool success = receive_data((char*)&rmsg, sizeof(rmsg));

	close_connection();

	return success ? rmsg.vcr_output : 1; /* default value is 1 (cf. controld.cpp) */
}

void CControldClient::Mute(const CControld::volume_type volume_type)
{
	setMute(true,volume_type);
}

void CControldClient::UnMute(const CControld::volume_type volume_type)
{
	setMute(false,volume_type);
}

void CControldClient::setMute(const bool mute, const CControld::volume_type volume_type)
{
	CControldMsg::commandMute msg;
	msg.mute = mute;
	msg.type = volume_type;
	send(CControldMsg::CMD_SETMUTE, (char*)&msg, sizeof(msg));
	close_connection();
}

bool CControldClient::getMute(const CControld::volume_type volume_type)
{
	CControldMsg::commandMute rmsg;
	rmsg.type = volume_type;
	send(CControldMsg::CMD_GETMUTESTATUS, (char*)&rmsg, sizeof(rmsg));
	receive_data((char*)&rmsg, sizeof(rmsg));
	close_connection();
	return rmsg.mute;
}

void CControldClient::registerEvent(unsigned int eventID, unsigned int clientID, const char * const udsName)
{
	CEventServer::commandRegisterEvent msg2;

	msg2.eventID = eventID;
	msg2.clientID = clientID;
	strcpy(msg2.udsName, udsName);

	send(CControldMsg::CMD_REGISTEREVENT, (char*)&msg2, sizeof(msg2));

	close_connection();
}

void CControldClient::unRegisterEvent(unsigned int eventID, unsigned int clientID)
{
	CEventServer::commandUnRegisterEvent msg2;

	msg2.eventID = eventID;
	msg2.clientID = clientID;

	send(CControldMsg::CMD_UNREGISTEREVENT, (char*)&msg2, sizeof(msg2));

	close_connection();
}

void CControldClient::videoPowerDown(bool powerdown)
{
        CControldMsg::commandVideoPowerSave msg2;

        msg2.powerdown = powerdown;

        send(CControldMsg::CMD_SETVIDEOPOWERDOWN, (char*)&msg2, sizeof(msg2));

        close_connection();
}

bool CControldClient::getVideoPowerDown()
{
	CControldMsg::responseVideoPowerSave msg;
	send(CControldMsg::CMD_GETVIDEOPOWERDOWN);
	receive_data((char*) &msg, sizeof(msg));
	close_connection();
	return msg.videoPowerSave;
}

void CControldClient::saveSettings()
{
        send(CControldMsg::CMD_SAVECONFIG);
        close_connection();
}

void CControldClient::setRGBCsync(char val)
{
	CControldMsg::commandCsync msg;
	msg.csync = val;
	send(CControldMsg::CMD_SETCSYNC, (char*) &msg, sizeof(msg));
	close_connection();
}

char CControldClient::getRGBCsync()
{
	CControldMsg::commandCsync msg;
	send(CControldMsg::CMD_GETCSYNC);
	receive_data((char*) &msg, sizeof(msg));
	close_connection();
	return msg.csync;
}
char CControldClient::getVideoMode()
{
        CControldMsg::responseVideoFormat rmsg;
        send(CControldMsg::CMD_GETVIDEOMODE);
        bool success = receive_data((char*)&rmsg, sizeof(rmsg));
        close_connection();
        return success ? rmsg.format : 1; /* default value is 2 (cf. controld.cpp) */
}
void CControldClient::setVideoMode(char format)
{
        CControldMsg::commandVideoFormat msg2;
        msg2.format = format;
        send(CControldMsg::CMD_SETVIDEOMODE, (char*)&msg2, sizeof(msg2));
        close_connection();
}

