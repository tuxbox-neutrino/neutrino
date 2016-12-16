/*
	Neutrino-GUI  -   DBoxII-Project

	Timerliste by Zwen
	(C) 2009, 2011-2014 Stefan Seyfried
	Remote Timers by
	(C) 2016            TangoCash

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/timerlist.h>
#include <gui/pluginlist.h>
#include <gui/plugins.h>

#include <daemonc/remotecontrol.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/fade.h>
#include <driver/record.h>
#include <driver/display.h>

#include <gui/channellist.h>
#include <gui/color.h>
#include <gui/epgview.h>
#include <gui/eventlist.h>
#include <gui/filebrowser.h>
#include <gui/infoviewer.h>

#include <gui/components/cc.h>
#include <gui/widget/buttons.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>
#include <gui/widget/keyboard_input.h>

#include <system/settings.h>
#include <system/fsmounter.h>
#include <system/helpers.h>
#include <system/httptool.h>

#include <json/json.h>

#include <global.h>
#include <neutrino.h>

#include <zapit/getservices.h>
#include <zapit/bouquets.h>
#include <zapit/femanager.h>

#include <timerdclient/timerdclient.h>

#include <eitd/sectionsd.h>

extern CBouquetManager *g_bouquetManager;

#include <string.h>

class CTimerListNewNotifier : public CChangeObserver
{
private:
	CMenuItem* m1;
	CMenuItem* m2;
	CMenuItem* m3;
	CMenuItem* m4;
	CMenuItem* m5;
	CMenuItem* m6;
	std::string * display;
	int* iType;
	time_t* stopTime;
public:
	CTimerListNewNotifier( int* Type, time_t* time,CMenuItem* a1, CMenuItem* a2,
			       CMenuItem* a3, CMenuItem* a4, CMenuItem* a5, CMenuItem* a6, std::string *d)
	{
		m1 = a1;
		m2 = a2;
		m3 = a3;
		m4 = a4;
		m5 = a5;
		m6 = a6;
		display=d;
		iType=Type;
		stopTime=time;
	}
	bool changeNotify(const neutrino_locale_t /*OptionName*/, void *)
	{
		CTimerd::CTimerEventTypes type = (CTimerd::CTimerEventTypes) *iType;
		if (type == CTimerd::TIMER_RECORD)
		{
			*stopTime=(time(NULL)/60)*60;
			struct tm *tmTime2 = localtime(stopTime);
			char disp[40];
			snprintf(disp, sizeof(disp), "%02d.%02d.%04d %02d:%02d", tmTime2->tm_mday, tmTime2->tm_mon+1,
				 tmTime2->tm_year+1900,
				 tmTime2->tm_hour, tmTime2->tm_min);
			*display = std::string(disp);
			m1->setActive(true);
			m6->setActive((g_settings.recording_type == RECORDING_FILE));
		}
		else
		{
			*stopTime=0;
			*display = "                ";
			m1->setActive (false);
			m6->setActive(false);
		}
		if (type == CTimerd::TIMER_RECORD ||
		    type == CTimerd::TIMER_ZAPTO)
				/*|| type == CTimerd::TIMER_NEXTPROGRAM)*/
		{
			m2->setActive(true);
		}
		else
		{
			m2->setActive(false);
		}
		if (type == CTimerd::TIMER_STANDBY)
			m3->setActive(true);
		else
			m3->setActive(false);
		if (type == CTimerd::TIMER_REMIND)
			m4->setActive(true);
		else
			m4->setActive(false);
		if (type == CTimerd::TIMER_EXEC_PLUGIN)
			m5->setActive(true);
		else
			m5->setActive(false);
		return true;
	}
};

class CTimerListRepeatNotifier : public CChangeObserver
{
private:
	CMenuForwarder* m1;
	CMenuForwarder* m2;

	int* iRepeat;
	std::string * weekdays;
public:
	CTimerListRepeatNotifier( int* repeat, CMenuForwarder* a1, CMenuForwarder *a2, std::string * wstr)
	{
		m1 = a1;
		m2 = a2;
		iRepeat=repeat;
		weekdays = wstr;
	}

	bool changeNotify(const neutrino_locale_t /*OptionName*/, void *)
	{
		if (*iRepeat >= (int)CTimerd::TIMERREPEAT_WEEKDAYS)
		{
			m1->setActive (true);
			*weekdays = "XXXXX--";
		}
		else
		{
			m1->setActive (false);
			*weekdays = "-------";
		}
		if (*iRepeat != (int)CTimerd::TIMERREPEAT_ONCE)
			m2->setActive(true);
		else
			m2->setActive(false);
		return true;
	}
};

class CTimerListApidNotifier : public CChangeObserver
{
private:
	int* o_dflt;
	int* o_std;
	int* o_alt;
	int* o_ac3;
	CMenuItem* m_dflt;
	CMenuItem* m_std;
	CMenuItem* m_alt;
	CMenuItem* m_ac3;
public:
	CTimerListApidNotifier( int* o1, int* o2, int* o3, int* o4)
	{
		o_dflt=o1;
		o_std=o2;
		o_alt=o3;
		o_ac3=o4;
	}

	void setItems(CMenuItem* m1, CMenuItem* m2, CMenuItem* m3, CMenuItem* m4)
	{
		m_dflt=m1;
		m_std=m2;
		m_alt=m3;
		m_ac3=m4;
	}

	bool changeNotify(const neutrino_locale_t OptionName, void *)
	{
		if (OptionName == LOCALE_TIMERLIST_APIDS_DFLT)
		{
			if (*o_dflt==0)
			{
				m_std->setActive(true);
				m_alt->setActive(true);
				m_ac3->setActive(true);
			}
			else
			{
				m_std->setActive(false);
				m_alt->setActive(false);
				m_ac3->setActive(false);
				*o_std=0;
				*o_alt=0;
				*o_ac3=0;
			}
		}
		else
		{
			if (*o_std || *o_alt || *o_ac3)
				*o_dflt=0;
		}
		return true;
	}
};

std::string string_printf_helper(const char *fmt, ...) {
	va_list arglist;
	const int bufferlen = 4*1024;
	char buffer[bufferlen] = {0};
	va_start(arglist, fmt);
	vsnprintf(buffer, bufferlen, fmt, arglist);
	va_end(arglist);
	return std::string(buffer);
}

CTimerList::CTimerList()
{
	frameBuffer = CFrameBuffer::getInstance();
	visible = false;
	x = y = 0;
	width = height = 0;
	fheight = theight = 0;
	footerHeight = 0;
	selected = 0;
	liststart = 0;
	listmaxshow = 0;
	Timer = new CTimerdClient();
	timerNew_message = "";
	timerNew_pluginName = "";
	httpConnectTimeout = 3;

	/* most probable default */
	saved_dispmode = (int)CVFD::MODE_TVRADIO;
}

CTimerList::~CTimerList()
{
	timerlist.clear();
	delete Timer;
}

int CTimerList::exec(CMenuTarget* parent, const std::string & actionKey)
{
	const char * key = actionKey.c_str();

	if (actionKey == "add_ip")
	{
		std::string rbname,rbaddress,user,pass = "";
		std::string port = "80";
		CKeyboardInput remotebox_name(LOCALE_REMOTEBOX_RBNAME, &rbname, 25);
		remotebox_name.forceSaveScreen(true);
		CKeyboardInput remotebox_address(LOCALE_REMOTEBOX_RBADDR, &rbaddress, 50);
		remotebox_address.forceSaveScreen(true);
		CStringInput remotebox_port(LOCALE_REMOTEBOX_PORT, &port, 5);
		remotebox_port.forceSaveScreen(true);
		CKeyboardInput remotebox_user(LOCALE_REMOTEBOX_USER, &user, 15);
		remotebox_user.forceSaveScreen(true);
		CKeyboardInput remotebox_pass(LOCALE_REMOTEBOX_PASS, &pass, 15);
		remotebox_pass.forceSaveScreen(true);
		CMenuWidget * rbsetup = new CMenuWidget(LOCALE_REMOTEBOX_HEAD, NEUTRINO_ICON_TIMER);
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_RBNAME, true, rbname, &remotebox_name));
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_RBADDR, true, rbaddress, &remotebox_address));
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_PORT, true, port, &remotebox_port));
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_USER, true, user, &remotebox_user));
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_PASS, true, pass, &remotebox_pass));
		rbsetup->enableSaveScreen(true);
		if ((rbsetup->exec(NULL,"") == true) && (!rbaddress.empty()))
		{
			remboxmenu->addItem(new CMenuForwarder(rbname, true, NULL, this, "cha_ip"));
			rbsetup->hide();
			remboxmenu->enableSaveScreen(false);
			remboxmenu->hide();
			timer_remotebox_item timer_rb;
			timer_rb.rbaddress = rbaddress;
			if (!timer_rb.rbaddress.empty())
			{
				timer_rb.port = atoi(port);
				timer_rb.user = user;
				timer_rb.pass = pass;
				timer_rb.rbname = rbname;
				if (timer_rb.rbname.empty())
					timer_rb.rbname = timer_rb.rbaddress;
				g_settings.timer_remotebox_ip.push_back(timer_rb);
			}
			changed = true;
		}
		return menu_return::RETURN_REPAINT;
	}

	if (actionKey == "del_ip")
	{
		bselected = remboxmenu->getSelected();
		if (bselected >= item_offset)
		{
			remboxmenu->removeItem(bselected);
			remboxmenu->enableSaveScreen(false);
			remboxmenu->hide();
			bselected = remboxmenu->getSelected();
			changed = true;
		}
		return menu_return::RETURN_REPAINT;
	}

	if (actionKey == "cha_ip")
	{
		bselected = remboxmenu->getSelected();
		CMenuItem* item = remboxmenu->getItem(bselected);
		CMenuForwarder *f = static_cast<CMenuForwarder*>(item);
		std::vector<timer_remotebox_item>::iterator it = g_settings.timer_remotebox_ip.begin();
		std::advance(it,bselected-item_offset);
		std::string port = to_string(it->port);
		CKeyboardInput remotebox_name(LOCALE_REMOTEBOX_RBNAME, &it->rbname, 25);
		remotebox_name.forceSaveScreen(true);
		CKeyboardInput remotebox_address(LOCALE_REMOTEBOX_RBADDR, &it->rbaddress, 50);
		remotebox_address.forceSaveScreen(true);
		CStringInput remotebox_port(LOCALE_REMOTEBOX_PORT, &port, 5);
		remotebox_port.forceSaveScreen(true);
		CKeyboardInput remotebox_user(LOCALE_REMOTEBOX_USER, &it->user, 15);
		remotebox_user.forceSaveScreen(true);
		CKeyboardInput remotebox_pass(LOCALE_REMOTEBOX_PASS, &it->pass, 15);
		remotebox_pass.forceSaveScreen(true);
		CMenuWidget * rbsetup = new CMenuWidget(LOCALE_REMOTEBOX_HEAD, NEUTRINO_ICON_TIMER);
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_RBNAME, true, it->rbname, &remotebox_name));
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_RBADDR, true, it->rbaddress, &remotebox_address));
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_PORT, true, port, &remotebox_port));
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_USER, true, it->user, &remotebox_user));
		rbsetup->addItem(new CMenuForwarder(LOCALE_REMOTEBOX_PASS, true, it->pass, &remotebox_pass));
		rbsetup->enableSaveScreen(true);
		if ((rbsetup->exec(NULL,"") == true) && (!it->rbaddress.empty()))
		{
			it->port = atoi(port);
			f->setName(it->rbname);
			rbsetup->hide();
			remboxmenu->enableSaveScreen(false);
			remboxmenu->hide();
			changed = true;
		}
		return menu_return::RETURN_REPAINT;
	}

	if (strcmp(key, "modifytimer") == 0)
	{
		timerlist[selected].announceTime = timerlist[selected].alarmTime -60;
		if (timerlist[selected].eventRepeat >= CTimerd::TIMERREPEAT_WEEKDAYS)
			Timer->getWeekdaysFromStr(&timerlist[selected].eventRepeat, m_weekdaysStr);
		if (timerlist[selected].eventType == CTimerd::TIMER_RECORD)
		{
			timerlist[selected].announceTime -= 120; // 2 more mins for rec timer
			if (timer_apids_dflt)
				timerlist[selected].apids = TIMERD_APIDS_CONF;
			else
				timerlist[selected].apids = (unsigned char)((timer_apids_std * TIMERD_APIDS_STD) | (timer_apids_ac3 * TIMERD_APIDS_AC3) | (timer_apids_alt * TIMERD_APIDS_ALT));
			Timer->modifyTimerAPid(timerlist[selected].eventID,timerlist[selected].apids);
			Timer->modifyRecordTimerEvent(timerlist[selected].eventID, timerlist[selected].announceTime,
						      timerlist[selected].alarmTime,
						      timerlist[selected].stopTime, timerlist[selected].eventRepeat,
						      timerlist[selected].repeatCount,timerlist[selected].recordingDir);
		}
		else if (timerlist[selected].eventType == CTimerd::TIMER_REMOTEBOX)
		{
			CHTTPTool httpTool;
			std::string r_url;
			r_url = "http://";
			r_url += RemoteBoxConnectUrl(timerlist[selected].remotebox_name);
			r_url += "/control/timer?action=new&update=1";
			r_url += "&alarm=" + to_string((int)timerlist[selected].alarmTime);
			r_url += "&stop=" + to_string((int)timerlist[selected].stopTime);
			r_url += "&announce=" + to_string((int)timerlist[selected].announceTime);
			r_url += "&channel_id=" + string_printf_helper(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timerlist[selected].channel_id);
			r_url += "&aj=on";
			r_url += "&rs=on";
			r_url += "&id=" + to_string((int)timerlist[selected].eventID);
			//printf("[remotetimer] url:%s\n",r_url.c_str());
			r_url = httpTool.downloadString(r_url, -1, httpConnectTimeout);
			//printf("[remotetimer] status:%s\n",r_url.c_str());
		}
		else
		{
			Timer->modifyTimerEvent(timerlist[selected].eventID, timerlist[selected].announceTime,
						timerlist[selected].alarmTime,
						timerlist[selected].stopTime, timerlist[selected].eventRepeat,
						timerlist[selected].repeatCount);
		}
		return menu_return::RETURN_EXIT;
	}
	else if ((strcmp(key, "send_remotetimer") == 0) && RemoteBoxChanExists(timerlist[selected].channel_id))
	{
		CHTTPTool httpTool;
		std::string r_url;
		r_url = "http://";
		r_url += RemoteBoxConnectUrl(timerlist[selected].remotebox_name);
		r_url += "/control/timer?action=new";
		r_url += "&alarm=" + to_string((int)timerlist[selected].alarmTime + timerlist[selected].rem_pre);
		r_url += "&stop=" + to_string((int)timerlist[selected].stopTime - timerlist[selected].rem_post);
		r_url += "&announce=" + to_string((int)timerlist[selected].announceTime);
		r_url += "&channel_id=" + string_printf_helper(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timerlist[selected].channel_id);
		r_url += "&aj=on";
		r_url += "&rs=on";
		//printf("[remotetimer] url:%s\n",r_url.c_str());
		r_url = httpTool.downloadString(r_url, -1, httpConnectTimeout);
		//printf("[remotetimer] status:%s\n",r_url.c_str());
		if (r_url=="ok")
			Timer->removeTimerEvent(timerlist[selected].eventID);
	}
	else if ((strcmp(key, "fetch_remotetimer") == 0) && LocalBoxChanExists(timerlist[selected].channel_id))
	{
		int pre,post;
		Timer->getRecordingSafety(pre,post);
		std::string remotebox_name = timerlist[selected].remotebox_name;
		std::string eventID = to_string((int)timerlist[selected].eventID);

		int res = Timer->addRecordTimerEvent(timerlist[selected].channel_id, timerlist[selected].alarmTime + pre,
				   timerlist[selected].stopTime - post, 0, 0, timerlist[selected].announceTime,
				   TIMERD_APIDS_CONF, true, timerlist[selected].announceTime > time(NULL),"",false);

		if (res == -1)
		{
			bool forceAdd = askUserOnTimerConflict(timerlist[selected].announceTime,timerlist[selected].stopTime);

			if (forceAdd)
			{
				res = Timer->addRecordTimerEvent(timerlist[selected].channel_id, timerlist[selected].alarmTime + pre,
				   timerlist[selected].stopTime - post, 0, 0, timerlist[selected].announceTime,
				   TIMERD_APIDS_CONF, true, timerlist[selected].announceTime > time(NULL),"",true);
			}
		}

		CHTTPTool httpTool;
		std::string r_url;
		r_url = "http://";
		r_url += RemoteBoxConnectUrl(remotebox_name);
		r_url += "/control/timer?action=remove";
		r_url += "&id=" + eventID;
		//printf("[remotetimer] url:%s\n",r_url.c_str());
		if (res > 0)
		r_url = httpTool.downloadString(r_url, -1, httpConnectTimeout);
		//printf("[remotetimer] status:%s\n",r_url.c_str());
	}
	else if (strcmp(key, "del_remotetimer") == 0)
	{
		CHTTPTool httpTool;
		std::string r_url;
		r_url = "http://";
		r_url += RemoteBoxConnectUrl(timerlist[selected].remotebox_name);
		r_url += "/control/timer?action=remove";
		r_url += "&id=" + to_string((int)timerlist[selected].eventID);
		//printf("[remotetimer] url:%s\n",r_url.c_str());
		r_url = httpTool.downloadString(r_url, -1, httpConnectTimeout);
		//printf("[remotetimer] status:%s\n",r_url.c_str());
	}
	else if (strcmp(key, "update_remotetimer") == 0)
	{
		CHTTPTool httpTool;
		std::string r_url;
		r_url = "http://";
		r_url += RemoteBoxConnectUrl(timerlist[selected].remotebox_name);
		r_url += "/control/timer?action=new&update=1";
		r_url += "&alarm=" + to_string((int)timerlist[selected].alarmTime);
		r_url += "&stop=" + to_string((int)timerlist[selected].stopTime);
		r_url += "&announce=" + to_string((int)timerlist[selected].announceTime);
		r_url += "&channel_id=" + string_printf_helper(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timerlist[selected].channel_id);
		r_url += "&aj=on";
		r_url += "&rs=on";
		//printf("[remotetimer] url:%s\n",r_url.c_str());
		r_url = httpTool.downloadString(r_url, -1, httpConnectTimeout);
		//printf("[remotetimer] status:%s\n",r_url.c_str());
	}
	else if (strcmp(key, "newtimer") == 0)
	{
		timerNew.announceTime=timerNew.alarmTime-60;
		CTimerd::EventInfo eventinfo;
		CTimerd::RecordingInfo recinfo;
		eventinfo.epgID=0;
		eventinfo.epg_starttime=0;
		eventinfo.channel_id=timerNew.channel_id;
		eventinfo.apids = TIMERD_APIDS_CONF;
		eventinfo.recordingSafety = false;
		eventinfo.autoAdjustToEPG = false;
		timerNew.standby_on = (timerNew_standby_on == 1);
		void *data=NULL;
		if (timerNew.eventType == CTimerd::TIMER_STANDBY)
			data=&(timerNew.standby_on);
		/* else if (timerNew.eventType==CTimerd::TIMER_NEXTPROGRAM || */
		else if (timerNew.eventType == CTimerd::TIMER_ZAPTO ||
			 timerNew.eventType == CTimerd::TIMER_RECORD)
		{
			if (timerNew_channel_name == "---")
				return menu_return::RETURN_REPAINT;
			if (timerNew.eventType==CTimerd::TIMER_RECORD)
			{
				recinfo.epgID=0;
				recinfo.epg_starttime=0;
				recinfo.channel_id=timerNew.channel_id;
				recinfo.apids=TIMERD_APIDS_CONF;
				recinfo.recordingSafety = false;
				recinfo.autoAdjustToEPG = false; // FIXME -- add GUI option?

				timerNew.announceTime-= 120; // 2 more mins for rec timer
				strncpy(recinfo.recordingDir,timerNew.recordingDir,sizeof(recinfo.recordingDir)-1);
				data = &recinfo;
			}
			else
				data= &eventinfo;
		}
		else if (timerNew.eventType==CTimerd::TIMER_REMIND)
		{
			if (timerNew_message == "---")
				return menu_return::RETURN_REPAINT;
			data = (void*)timerNew_message.c_str();
		}
		else if (timerNew.eventType==CTimerd::TIMER_EXEC_PLUGIN)
		{
			if (timerNew_pluginName == "---")
				return menu_return::RETURN_REPAINT;
			data = (void*)timerNew_pluginName.c_str();
		}
		if (timerNew.eventRepeat >= CTimerd::TIMERREPEAT_WEEKDAYS)
			Timer->getWeekdaysFromStr(&timerNew.eventRepeat, m_weekdaysStr);

		if (Timer->addTimerEvent(timerNew.eventType,data,timerNew.announceTime,timerNew.alarmTime,
					 timerNew.stopTime,timerNew.eventRepeat,timerNew.repeatCount,false) == -1)
		{
			bool forceAdd = askUserOnTimerConflict(timerNew.announceTime,timerNew.stopTime);

			if (forceAdd)
			{
				Timer->addTimerEvent(timerNew.eventType,data,timerNew.announceTime,timerNew.alarmTime,
						     timerNew.stopTime, timerNew.eventRepeat,timerNew.repeatCount,true);
			}
		}
		return menu_return::RETURN_EXIT;
	}
	else if (strncmp(key, "SC:", 3) == 0)
	{
		int delta;
		sscanf(&(key[3]),
			SCANF_CHANNEL_ID_TYPE
			"%n",
			&timerNew.channel_id,
			&delta);
		timerNew_channel_name = std::string(key + 3 + delta + 1);
		g_RCInput->postMsg(CRCInput::RC_timeout, 0); // leave underlying menu also
		g_RCInput->postMsg(CRCInput::RC_timeout, 0); // leave underlying menu also
		return menu_return::RETURN_EXIT;
	}
	else if (actionKey == "rec_dir1")
	{
		if (parent)
			parent->hide();
		const char *action_str = "RecDir1";
		if (chooserDir(timerlist[selected].recordingDir, true, action_str, sizeof(timerlist[selected].recordingDir)-1))
		{
			printf("[timerlist] new %s dir %s\n", action_str, timerlist[selected].recordingDir);
		}
		timer_recordingDir = timerlist[selected].recordingDir;
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "rec_dir2")
	{
		if (parent)
			parent->hide();
		const char *action_str = "RecDir2";
		if (chooserDir(timerNew.recordingDir, true, action_str, sizeof(timerNew.recordingDir)-1))
		{
			printf("[timerlist] new %s dir %s\n", action_str, timerNew.recordingDir);
		}
		timerNew_recordingDir = timerNew.recordingDir;
		return menu_return::RETURN_REPAINT;
	}
	if (parent)
	{
		parent->hide();
	}

	int ret = show();
	CVFD::getInstance()->setMode((CVFD::MODES)saved_dispmode);

	return ret;
	/*
		if (ret > -1)
		{
			return menu_return::RETURN_REPAINT;
		}
		else if (ret == -1)
		{
			// -1 bedeutet nur REPAINT
			return menu_return::RETURN_REPAINT;
		}
		else
		{
			// -2 bedeutet EXIT_ALL
			return menu_return::RETURN_EXIT_ALL;
		}*/
}

struct button_label TimerListButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED   	, LOCALE_TIMERLIST_DELETE },
	{ NEUTRINO_ICON_BUTTON_GREEN 	, LOCALE_TIMERLIST_NEW    },
	{ NEUTRINO_ICON_BUTTON_YELLOW	, LOCALE_TIMERLIST_RELOAD },
	{ NEUTRINO_ICON_BUTTON_BLUE	, LOCALE_TIMERLIST_MODIFY },
	{ NEUTRINO_ICON_BUTTON_INFO_SMALL, NONEXISTANT_LOCALE     },
	{ NEUTRINO_ICON_BUTTON_PLAY	, NONEXISTANT_LOCALE      }
};
size_t TimerListButtonsCount = sizeof(TimerListButtons)/sizeof(TimerListButtons[0]);

#define RemoteBoxFooterButtonCount 3
static const struct button_label RemoteBoxFooterButtons[RemoteBoxFooterButtonCount] = {
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_REMOTEBOX_DEL },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_REMOTEBOX_ADD },
	{ NEUTRINO_ICON_BUTTON_OKAY, LOCALE_REMOTEBOX_MOD }
};

void CTimerList::updateEvents(void)
{
	timerlist.clear();
	Timer->getTimerList (timerlist);
	RemoteBoxTimerList (timerlist);
	sort(timerlist.begin(), timerlist.end());

	theight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	//get footerHeight from paintButtons
	footerHeight = ::paintButtons(TimerListButtons, TimerListButtonsCount, 0, 0, 0, 0, 0, false);

	//width = w_max(g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getWidth()*56, 20);
	width = frameBuffer->getScreenWidth()*0.9;
	height = frameBuffer->getScreenHeight() - (2*theight);	// max height

	listmaxshow = (height-theight)/(fheight*2);
	height = theight+listmaxshow*fheight*2+footerHeight;	// recalc height

	if (timerlist.size() < listmaxshow)
	{
		listmaxshow=timerlist.size();
		height = theight+listmaxshow*fheight*2+footerHeight;	// recalc height
	}

	if (!timerlist.empty() && selected == (int)timerlist.size())
	{
		selected=timerlist.size()-1;
		liststart = (selected/listmaxshow)*listmaxshow;
	}

	x = getScreenStartX(width);
	y = getScreenStartY(height);
}

void CTimerList::RemoteBoxSelect()
{
	int select = 0;
	CMenuWidget *m = new CMenuWidget(LOCALE_REMOTEBOX_HEAD, NEUTRINO_ICON_TIMER);
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

	for (std::vector<timer_remotebox_item>::iterator it = g_settings.timer_remotebox_ip.begin(); it != g_settings.timer_remotebox_ip.end(); ++it)
		m->addItem(new CMenuForwarder(it->rbname, true, NULL, selector, to_string(std::distance(g_settings.timer_remotebox_ip.begin(),it)).c_str()));

	m->enableSaveScreen(true);
	m->exec(NULL, "");

	delete selector;

	std::vector<timer_remotebox_item>::iterator it = g_settings.timer_remotebox_ip.begin();
	std::advance(it,select);
	if (askUserOnRemoteTimerConflict(timerlist[selected].announceTime, timerlist[selected].stopTime, (char*) it->rbname.c_str()))
	{
		strncpy(timerlist[selected].remotebox_name,it->rbname.c_str(),sizeof(timerlist[selected].remotebox_name));
		timerlist[selected].remotebox_name[sizeof(timerlist[selected].remotebox_name) - 1] = 0;
	}
}

bool CTimerList::RemoteBoxChanExists(t_channel_id channel_id)
{
	if (strcmp(timerlist[selected].remotebox_name,"") == 0)
		return false;

	CHTTPTool httpTool;
	std::string r_url;
	r_url = "http://";
	r_url += RemoteBoxConnectUrl(timerlist[selected].remotebox_name);
	r_url += "/control/getchannel?format=json&id=";
	r_url += string_printf_helper(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id);
	r_url = httpTool.downloadString(r_url, -1, httpConnectTimeout);

	Json::Value root;
	Json::Reader reader;
	bool parsedSuccess = reader.parse(r_url, root, false);
	if (!parsedSuccess) {
		printf("Failed to parse JSON\n");
		printf("%s\n", reader.getFormattedErrorMessages().c_str());
	}

	r_url = root.get("success","false").asString();

	if (r_url == "false")
		ShowMsg(LOCALE_REMOTEBOX_CHANNEL_NA, convertChannelId2String(channel_id),
				CMsgBox::mbrOk, CMsgBox::mbOk, NULL, 450, 30, false);

	return (r_url == "true");
}

bool CTimerList::LocalBoxChanExists(t_channel_id channel_id)
{
	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
	if (channel)
		return true;
	else
		return false;
}

std::string CTimerList::RemoteBoxConnectUrl(std::string _rbname)
{
	std::string c_url = "";
	for (std::vector<timer_remotebox_item>::iterator it = g_settings.timer_remotebox_ip.begin(); it != g_settings.timer_remotebox_ip.end(); ++it)
	{
		if (it->rbname == _rbname)
		{
			if (!it->user.empty() && !it->pass.empty())
				c_url += it->user + ":" + it->pass +"@";
			c_url += it->rbaddress;
			c_url += ":" + to_string(it->port);
			break;
		}
	}
	return c_url;
}

void CTimerList::RemoteBoxTimerList(CTimerd::TimerList &rtimerlist)
{
	if (g_settings.timer_remotebox_ip.size() == 0)
		return;

	CHTTPTool httpTool;
	std::string r_url;
	for (std::vector<timer_remotebox_item>::iterator it = g_settings.timer_remotebox_ip.begin(); it != g_settings.timer_remotebox_ip.end(); ++it)
	{
		r_url = "http://";
		r_url += RemoteBoxConnectUrl(it->rbname);
		r_url += "/control/timer?format=json";
		r_url = httpTool.downloadString(r_url, -1, httpConnectTimeout);
		//printf("[remotetimer] timers:%s\n",r_url.c_str());

		Json::Value root;
		Json::Reader reader;
		bool parsedSuccess = reader.parse(r_url, root, false);
		if (!parsedSuccess)
		{
			printf("Failed to parse JSON\n");
			printf("%s\n", reader.getFormattedErrorMessages().c_str());
		}
		Json::Value delays = root["data"]["timer"][0];

		rem_pre  = atoi(delays["config"].get("pre_delay","0").asString());
		rem_post = atoi(delays["config"].get("post_delay","0").asString());

		//printf("[remotetimer] pre:%d - post:%d\n", rem_pre, rem_post);

		Json::Value remotetimers = root["data"]["timer"][0]["timer_list"];

		for (unsigned int i= 0; i<remotetimers.size();i++)
		{
			CTimerd::responseGetTimer rtimer;
			if ( atoi(remotetimers[i].get("type_number","").asString()) == 5)
			{
				strncpy(rtimer.remotebox_name,it->rbname.c_str(),sizeof(rtimer.remotebox_name));
				rtimer.remotebox_name[sizeof(rtimer.remotebox_name) - 1] = 0;
				rtimer.rem_pre = rem_pre;
				rtimer.rem_post = rem_post;
				rtimer.eventID = atoi(remotetimers[i].get("id","").asString());
				rtimer.eventType = CTimerd::TIMER_REMOTEBOX;
				rtimer.eventState = (CTimerd::CTimerEventStates) atoi(remotetimers[i].get("state","").asString());
				if ( remotetimers[i]["repeat"].get("count","").asString() == "-")
					rtimer.repeatCount = 0;
				else
					rtimer.repeatCount = atoi(remotetimers[i]["repeat"].get("count","").asString());
				rtimer.eventRepeat = (CTimerd::CTimerEventRepeat)(atoi(remotetimers[i]["repeat"].get("number","").asString()) & 0x1FF);
				std::string wd = remotetimers[i]["repeat"].get("weekdays","").asString();
				CTimerdClient().getWeekdaysFromStr(&rtimer.eventRepeat, wd);
				rtimer.alarmTime = (time_t) atoll(remotetimers[i]["alarm"][0].get("digits","").asString().c_str());
				rtimer.announceTime = (time_t) atoll(remotetimers[i]["announce"][0].get("digits","").asString().c_str());
				rtimer.stopTime = (time_t) atoll(remotetimers[i]["stop"][0].get("digits","").asString().c_str());
				rtimer.epgID = (event_id_t) atoi(remotetimers[i].get("epg_id","").asString());
				sscanf(remotetimers[i].get("channel_id","").asString().c_str(),	SCANF_CHANNEL_ID_TYPE, &rtimer.channel_id);
				strncpy(rtimer.epgTitle,remotetimers[i].get("title","").asString().c_str(),51);
				if (remotetimers[i]["audio"].get("apids_conf","").asString() == "true")
					rtimer.apids = TIMERD_APIDS_CONF;
				//printf("[remotetimer] r-timer:%s - %s\n", remotetimers[i].get("channel_id","").asString().c_str(), remotetimers[i].get("title","").asString().c_str());
				rtimerlist.push_back(rtimer);
			}
		}
	}
}

int CTimerList::show()
{
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop=true;
	bool update=true;

	COSDFader fader(g_settings.theme.menu_Content_alpha);
	fader.StartFadeIn();

	while (loop)
	{
		if (update)
		{
			hide();
			updateEvents();
			update=false;
			paint();
		}
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );
		
		//ignore numeric keys
		if (g_RCInput->isNumeric(msg))
		{
			msg = CRCInput::RC_nokey;
		}
			
		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if ((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer()))
		{
			if (fader.FadeDone())
				loop = false;
		}
		else if (
			   (msg == CRCInput::RC_timeout)
			|| (msg == CRCInput::RC_home)
			|| (msg == CRCInput::RC_left)
			|| (msg == CRCInput::RC_ok && timerlist.empty())
		)
		{	//Exit after timeout or cancel key
			if (fader.StartFadeOut())
			{
				timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
				msg = 0;
			}
			else
				loop=false;

		}
		else if (!timerlist.empty() && (
				   msg == CRCInput::RC_up
				|| msg == CRCInput::RC_down
				|| (int)msg == g_settings.key_pageup
				|| (int)msg == g_settings.key_pagedown
			)
		)
		{
			int prev_selected = selected;
			int oldliststart = liststart;
			selected = UpDownKey(timerlist, msg, listmaxshow, selected);
			if (selected < 0) /* UpDownKey error */
				selected = prev_selected;
			liststart = (selected / listmaxshow) * listmaxshow;
			if (oldliststart != liststart)
				paint();
			else
			{
				paintItem(prev_selected - liststart);
				paintItem(selected - liststart);
			}
			paintFoot();
		}
		else if (!timerlist.empty() && (
				   msg == CRCInput::RC_ok
				|| msg == CRCInput::RC_right
				|| msg == CRCInput::RC_blue
			)
		)
		{
			if (modifyTimer()==menu_return::RETURN_EXIT_ALL)
			{
				res=menu_return::RETURN_EXIT_ALL;
				loop=false;
			}
			else
				update=true;
		}
		else if (!timerlist.empty() && (msg == CRCInput::RC_play && g_settings.timer_remotebox_ip.size() > 0))
		{
			if (timerlist[selected].eventType == CTimerd::TIMER_RECORD )
			{
				RemoteBoxSelect();
				if (exec(this,"send_remotetimer"))
				{
					res = menu_return::RETURN_EXIT_ALL;
					loop = false;
				}
				else
					update=true;
			}
			else if (timerlist[selected].eventType == CTimerd::TIMER_REMOTEBOX )
			{
				if (exec(this,"fetch_remotetimer"))
				{
					res = menu_return::RETURN_EXIT_ALL;
					loop = false;
				}
				else
					update = true;
			}
		}
		else if (!timerlist.empty() && msg == CRCInput::RC_red)
		{
			if ((timerlist[selected].eventType == CTimerd::TIMER_REMOTEBOX) && (timerlist[selected].eventState < CTimerd::TIMERSTATE_ISRUNNING))
			{
				if (exec(this,"del_remotetimer"))
				{
					res = menu_return::RETURN_EXIT_ALL;
					loop = false;
				}
			}
			else
			{
				bool killTimer = true;
				if (CRecordManager::getInstance()->RecordingStatus(timerlist[selected].channel_id))
				{
					CTimerd::RecordingStopInfo recinfo;
					recinfo.channel_id = timerlist[selected].channel_id;
					recinfo.eventID = timerlist[selected].eventID;
					if (CRecordManager::getInstance()->IsRecording(&recinfo))
					{
						std::string title = "";
						char buf1[1024];
						CEPGData epgdata;
						CEitManager::getInstance()->getEPGid(timerlist[selected].epgID, timerlist[selected].epg_starttime, &epgdata);
						memset(buf1, '\0', sizeof(buf1));
						if (!epgdata.title.empty())
							title = "(" + epgdata.title + ")\n";
						snprintf(buf1, sizeof(buf1)-1, g_Locale->getText(LOCALE_TIMERLIST_ASK_TO_DELETE), title.c_str());
						if (ShowMsg(LOCALE_RECORDINGMENU_RECORD_IS_RUNNING, buf1, CMsgBox::mbrNo, CMsgBox::mbYesNo, NULL, 450) & CMsgBox::mbrNo)
						{
							killTimer = false;
							update = false;
						}
					}
				}
				if (killTimer)
				{
					Timer->removeTimerEvent(timerlist[selected].eventID);
					update = true;
				}
			}
		}
		else if (msg == CRCInput::RC_green)
		{
			if (newTimer() == menu_return::RETURN_EXIT_ALL)
			{
				res = menu_return::RETURN_EXIT_ALL;
				loop = false;
			}
			else
				update = true;
		}
		else if (msg == CRCInput::RC_setup)
		{
			update = RemoteBoxSetup();
		}
		else if (msg == CRCInput::RC_yellow)
		{
			update = true;
		}
#if 0
		else if (msg==CRCInput::RC_blue || CRCInput::isNumeric(msg))
		{
			//pushback key if...
			g_RCInput->postMsg( msg, data );
			loop=false;
		}
#endif
		else if ( msg == CRCInput::RC_help || msg == CRCInput::RC_info)
		{
			CTimerd::responseGetTimer* timer=&timerlist[selected];
			if (timer!=NULL)
			{
				if (timer->eventType == CTimerd::TIMER_RECORD || timer->eventType == CTimerd::TIMER_REMOTEBOX || timer->eventType == CTimerd::TIMER_ZAPTO)
				{
					hide();
					if (timer->epgID != 0)
					{
						res = g_EpgData->show(timer->channel_id, timer->epgID, &timer->epg_starttime);
						update = true;
					}
					else
						ShowHint(LOCALE_MESSAGEBOX_INFO, LOCALE_EPGVIEWER_NOTFOUND);
					if (res == menu_return::RETURN_EXIT_ALL)
						loop = false;
					else
						paint();
				}
			}
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			g_RCInput->postMsg(msg, 0);
			loop = false;
			res = menu_return::RETURN_EXIT_ALL;
		}
		else
		{
			if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
			{
				loop = false;
				res = menu_return::RETURN_EXIT_ALL;
			}
		}
	}
	hide();
	fader.StopFade();

	return(res);
}

void CTimerList::hide()
{
	if (visible)
	{
		frameBuffer->paintBackgroundBoxRel(x, y, width + OFFSET_SHADOW, height + OFFSET_SHADOW);
		visible = false;
	}
}

bool CTimerList::RemoteBoxSetup()
{
	bool ret = false;
	remboxmenu = new CMenuWidget(LOCALE_REMOTEBOX_HEAD, NEUTRINO_ICON_TIMER);
	remboxmenu->addKey(CRCInput::RC_red, this, "del_ip");
	remboxmenu->addKey(CRCInput::RC_green, this, "add_ip");

	remboxmenu->addIntroItems();

	item_offset = remboxmenu->getItemsCount();
	for (std::vector<timer_remotebox_item>::iterator it = g_settings.timer_remotebox_ip.begin(); it != g_settings.timer_remotebox_ip.end(); ++it)
		remboxmenu->addItem(new CMenuForwarder(it->rbname, true, NULL, this, "cha_ip"));

	remboxmenu->setFooter(RemoteBoxFooterButtons, RemoteBoxFooterButtonCount);

	remboxmenu->enableSaveScreen(true);
	remboxmenu->exec(NULL, "");
	if (changed)
	{
		std::vector<timer_remotebox_item> old_timer_remotebox_ip = g_settings.timer_remotebox_ip;
		g_settings.timer_remotebox_ip.clear();
		for (int i = item_offset; i < remboxmenu->getItemsCount(); i++)
		{
			CMenuItem *item = remboxmenu->getItem(i);
			CMenuForwarder *f = static_cast<CMenuForwarder*>(item);
			for (std::vector<timer_remotebox_item>::iterator it = old_timer_remotebox_ip.begin(); it != old_timer_remotebox_ip.end(); ++it)
				if (it->rbname == f->getName())
				{
					g_settings.timer_remotebox_ip.push_back(*it);
				}
		}
		changed = false;
		ret = true;
	}
	delete remboxmenu;
	return ret;
}

void CTimerList::paintItem(int pos)
{
	int ypos = y+ theight+ pos*fheight*2;

	int real_width=width;
	if (timerlist.size() > listmaxshow)
	{
		real_width-=15; //scrollbar
	}

	unsigned int currpos = liststart + pos;

	bool i_selected	= currpos == (unsigned) selected;
	bool i_marked	= false;
	bool i_switch	= false; //pos & 1;
	int i_radius	= RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected, i_marked, i_switch);

	if (i_selected || i_marked)
		i_radius = RADIUS_LARGE;

	if (i_radius)
		frameBuffer->paintBoxRel(x, ypos, real_width, 2*fheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos, real_width, 2*fheight, bgcolor, i_radius);
	//shadow
	frameBuffer->paintBoxRel(x + width, ypos, OFFSET_SHADOW, 2*fheight, COL_SHADOW_PLUS_0);

	if (currpos < timerlist.size())
	{
		CTimerd::responseGetTimer & timer = timerlist[currpos];
		char zAlarmTime[25] = {0};
		struct tm *alarmTime = localtime(&(timer.alarmTime));
		strftime(zAlarmTime,20,"%d.%m. %H:%M",alarmTime);
		char zStopTime[25] = {0};
		struct tm *stopTime = localtime(&(timer.stopTime));
		strftime(zStopTime,20,"%d.%m. %H:%M",stopTime);
		int fw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getWidth();
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+10,ypos+fheight, fw*12, zAlarmTime, color, fheight);
		if (timer.stopTime != 0)
		{
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+10,ypos+2*fheight, fw*12, zStopTime, color, fheight);
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+fw*13,ypos+fheight, (real_width-fw*13)/2-5, convertTimerRepeat2String(timer.eventRepeat), color, fheight);

		if (timer.eventRepeat != CTimerd::TIMERREPEAT_ONCE)
		{
			char srepeatcount[25] = {0};
			if (timer.repeatCount == 0)
			{
				// Unicode 8734 (hex: 221E) not available in all fonts
				//sprintf(srepeatcount,"∞");
				sprintf(srepeatcount,"00");
			}
			else
				sprintf(srepeatcount,"%ux",timer.repeatCount);
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+fw*13+(real_width-fw*23)/2,ypos+fheight, (real_width-fw*13)/2-5, srepeatcount, color, fheight);
		}
		std::string t_type = (timer.eventType == CTimerd::TIMER_REMOTEBOX) ? std::string(convertTimerType2String(timer.eventType)) + " (" + std::string(timer.remotebox_name) + ")" : convertTimerType2String(timer.eventType);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+fw*13+(real_width-fw*13)/2,ypos+fheight, (real_width-fw*13)/2-5, t_type, color, fheight);

		// paint rec icon when recording in progress
		if ((timer.eventType == CTimerd::TIMER_RECORD) && (CRecordManager::getInstance()->RecordingStatus(timer.channel_id)))
		{
			CTimerd::RecordingStopInfo recinfo;
			recinfo.channel_id = timer.channel_id;
			recinfo.eventID = timer.eventID;
			if (CRecordManager::getInstance()->IsRecording(&recinfo))
			{
				int icol_w, icol_h;
				frameBuffer->getIconSize(NEUTRINO_ICON_REC, &icol_w, &icol_h);
				if ((icol_w > 0) && (icol_h > 0))
				{
					frameBuffer->paintIcon(NEUTRINO_ICON_REC, (x + real_width) - (icol_w + 8), ypos, 2*fheight);
				}
			}
		}

		if ((timer.eventType == CTimerd::TIMER_REMOTEBOX) && timer.eventState == CTimerd::TIMERSTATE_ISRUNNING)
		{
				int icol_w, icol_h;
				frameBuffer->getIconSize(NEUTRINO_ICON_REC, &icol_w, &icol_h);
				if ((icol_w > 0) && (icol_h > 0))
				{
					frameBuffer->paintIcon(NEUTRINO_ICON_REC, (x + real_width) - (icol_w + 8), ypos, 2*fheight);
				}
		}

		std::string zAddData("");
		switch (timer.eventType)
		{
		//case CTimerd::TIMER_NEXTPROGRAM :
		case CTimerd::TIMER_ZAPTO :
		case CTimerd::TIMER_RECORD :
		{
			zAddData = convertChannelId2String(timer.channel_id); // UTF-8
			if (timer.apids != TIMERD_APIDS_CONF)
			{
				std::string sep = "";
				zAddData += " (";
				if (timer.apids & TIMERD_APIDS_STD)
				{
					zAddData += "STD";
					sep = "/";
				}
				if (timer.apids & TIMERD_APIDS_ALT)
				{
					zAddData += sep;
					zAddData += "ALT";
					sep = "/";
				}
				if (timer.apids & TIMERD_APIDS_AC3)
				{
					zAddData += sep;
					zAddData += "AC3";
//					sep = "/";
				}
				zAddData += ')';
			}
			if (timer.epgID!=0)
			{
				CEPGData epgdata;
				if (CEitManager::getInstance()->getEPGid(timer.epgID, timer.epg_starttime, &epgdata))
				{
					zAddData += " : ";
					zAddData += epgdata.title;
				}
				else if (strlen(timer.epgTitle)!=0)
				{
					zAddData += " : ";
					zAddData += timer.epgTitle;
				}
			}
			else if (strlen(timer.epgTitle)!=0)
			{
				zAddData += " : ";
				zAddData += timer.epgTitle;
			}
		}
		break;
		case CTimerd::TIMER_REMOTEBOX :
		{
			CHTTPTool httpTool;
			std::string r_url;
			r_url = "http://";
			r_url += RemoteBoxConnectUrl(timer.remotebox_name);
			r_url += "/control/getchannel?format=json&id=";
			r_url += string_printf_helper(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer.channel_id);
			r_url = httpTool.downloadString(r_url, -1, httpConnectTimeout);

			Json::Value root;
			Json::Reader reader;
			bool parsedSuccess = reader.parse(r_url, root, false);
			if (!parsedSuccess)
			{
				printf("Failed to parse JSON\n");
				printf("%s\n", reader.getFormattedErrorMessages().c_str());
			}

			Json::Value remotechannel = root["data"]["channel"][0];

			zAddData = remotechannel.get("name","").asString();
			if (timer.apids != TIMERD_APIDS_CONF)
			{
				std::string sep = "";
				zAddData += " (";
				if (timer.apids & TIMERD_APIDS_STD)
				{
					zAddData += "STD";
					sep = "/";
				}
				if (timer.apids & TIMERD_APIDS_ALT)
				{
					zAddData += sep;
					zAddData += "ALT";
					sep = "/";
				}
				if (timer.apids & TIMERD_APIDS_AC3)
				{
					zAddData += sep;
					zAddData += "AC3";
//					sep = "/";
				}
				zAddData += ')';
			}
			if (strlen(timer.epgTitle)!=0)
			{
				zAddData += " : ";
				zAddData += timer.epgTitle;
			}
		}
		break;
		case CTimerd::TIMER_STANDBY:
		{
			zAddData = g_Locale->getText(timer.standby_on ? LOCALE_TIMERLIST_STANDBY_ON : LOCALE_TIMERLIST_STANDBY_OFF);
			break;
		}
		case CTimerd::TIMER_REMIND :
		{
			zAddData = timer.message; // must be UTF-8 encoded !
		}
		break;
		case CTimerd::TIMER_EXEC_PLUGIN :
		{
			zAddData = timer.pluginName;
		}
		break;
		default:
		{}
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+fw*13,ypos+2*fheight, real_width-(fw*13+5), zAddData, color, fheight);
		// LCD Display
		if (currpos == (unsigned) selected)
		{
			std::string line1 = convertTimerType2String(timer.eventType); // UTF-8
			//std::string line2 = zAlarmTime;
			switch (timer.eventType)
			{
			case CTimerd::TIMER_RECORD :
			//	line2+= " -";
			//	line2+= zStopTime+6;
			//case CTimerd::TIMER_NEXTPROGRAM :
			case CTimerd::TIMER_ZAPTO :
			{
				line1 += ' ';
				line1 += convertChannelId2String(timer.channel_id); // UTF-8
			}
			break;
			case CTimerd::TIMER_STANDBY :
			{
				if (timer.standby_on)
					line1+=" ON";
				else
					line1+=" OFF";
			}
			break;
			default:
				;
			}
			CVFD::getInstance()->showMenuText(0, line1.c_str(), -1, true); // UTF-8
			//CVFD::getInstance()->showMenuText(1, line2.c_str(), -1, true); // UTF-8
		}
	}
}

void CTimerList::paintHead()
{
	CComponentsHeaderLocalized header(x, y, width, theight, LOCALE_TIMERLIST_NAME, NEUTRINO_ICON_TIMER, CComponentsHeader::CC_BTN_MENU | CComponentsHeader::CC_BTN_EXIT, NULL, CC_SHADOW_ON);
	header.enableClock(true, "%d.%m.%Y  %H:%M");
	header.paint(CC_SAVE_SCREEN_NO);
}

void CTimerList::paintFoot()
{
	if(!timerlist.empty() )
	{
		CTimerd::responseGetTimer* timer=&timerlist[selected];
		if (timer != NULL)
		{
		//replace info button with dummy if timer is not type REC or ZAP
			if (timer->eventType == CTimerd::TIMER_RECORD || timer->eventType == CTimerd::TIMER_REMOTEBOX || timer->eventType == CTimerd::TIMER_ZAPTO)
				TimerListButtons[4].button = NEUTRINO_ICON_BUTTON_INFO_SMALL;
			else
				TimerListButtons[4].button = NEUTRINO_ICON_BUTTON_DUMMY_SMALL;
		}
	}
	//shadow
	frameBuffer->paintBoxRel(x + OFFSET_SHADOW, y + height - footerHeight, width, footerHeight + OFFSET_SHADOW, COL_SHADOW_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	int c = TimerListButtonsCount;
	if (g_settings.timer_remotebox_ip.size() == 0)
		c--; // reduce play button

	if (timerlist.empty())
		::paintButtons(x, y + height - footerHeight, width, 2, &(TimerListButtons[1]), width);
	else
		::paintButtons(x, y + height - footerHeight, width, c, TimerListButtons, width);
}

void CTimerList::paint()
{
	unsigned int page_nr = (listmaxshow == 0) ? 0 : (selected / listmaxshow);
	liststart = page_nr * listmaxshow;

	saved_dispmode = (int)CVFD::getInstance()->getMode();
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, g_Locale->getText(LOCALE_TIMERLIST_NAME));

	paintHead();
	for (unsigned int count=0; count<listmaxshow; count++)
	{
		paintItem(count);
	}

	if (timerlist.size()>listmaxshow)
	{
		int ypos = y+ theight;
		int sb = 2*fheight* listmaxshow;
		frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb, COL_SCROLLBAR_PASSIVE_PLUS_0);
		unsigned int tmp_max = listmaxshow;
		if (!tmp_max)
			tmp_max = 1;
		int sbc = ((timerlist.size()- 1)/ tmp_max)+ 1;

		frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ page_nr * (sb-4)/sbc, 11, (sb-4)/sbc, COL_SCROLLBAR_ACTIVE_PLUS_0, RADIUS_SMALL);
	}

	paintFoot();
	visible = true;
}

const char * CTimerList::convertTimerType2String(const CTimerd::CTimerEventTypes type) // UTF-8
{
	switch (type)
	{
	case CTimerd::TIMER_SHUTDOWN    :
		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_SHUTDOWN   );
//	case CTimerd::TIMER_NEXTPROGRAM :
//		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_NEXTPROGRAM);
	case CTimerd::TIMER_ZAPTO       :
		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_ZAPTO      );
	case CTimerd::TIMER_STANDBY     :
		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_STANDBY    );
	case CTimerd::TIMER_RECORD      :
		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_RECORD     );
	case CTimerd::TIMER_REMIND      :
		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_REMIND     );
	case CTimerd::TIMER_SLEEPTIMER  :
		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_SLEEPTIMER );
	case CTimerd::TIMER_EXEC_PLUGIN :
		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_EXECPLUGIN );
	case CTimerd::TIMER_REMOTEBOX   :
		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_REMOTEBOX  );
	default                         :
		return g_Locale->getText(LOCALE_TIMERLIST_TYPE_UNKNOWN    );
	}
}

std::string CTimerList::convertTimerRepeat2String(const CTimerd::CTimerEventRepeat rep) // UTF-8
{
	switch (rep)
	{
	case CTimerd::TIMERREPEAT_ONCE               :
		return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_ONCE              );
	case CTimerd::TIMERREPEAT_DAILY              :
		return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_DAILY             );
	case CTimerd::TIMERREPEAT_WEEKLY             :
		return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_WEEKLY            );
	case CTimerd::TIMERREPEAT_BIWEEKLY           :
		return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_BIWEEKLY          );
	case CTimerd::TIMERREPEAT_FOURWEEKLY         :
		return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_FOURWEEKLY        );
	case CTimerd::TIMERREPEAT_MONTHLY            :
		return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_MONTHLY           );
	case CTimerd::TIMERREPEAT_BYEVENTDESCRIPTION :
		return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_BYEVENTDESCRIPTION);
	default:
		if (rep >=CTimerd::TIMERREPEAT_WEEKDAYS)
		{
			int weekdays = (((int)rep) >> 9);
			std::string weekdayStr="";
			if (weekdays & 1)
				weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_MONDAY);
			weekdays >>= 1;
			if (weekdays & 1)
				weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_TUESDAY);
			weekdays >>= 1;
			if (weekdays & 1)
				weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_WEDNESDAY);
			weekdays >>= 1;
			if (weekdays & 1)
				weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_THURSDAY);
			weekdays >>= 1;
			if (weekdays & 1)
				weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_FRIDAY);
			weekdays >>= 1;
			if (weekdays & 1)
				weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_SATURDAY);
			weekdays >>= 1;
			if (weekdays & 1)
				weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_SUNDAY);
			return weekdayStr;
		}
		else
			return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_UNKNOWN);
	}
}

std::string CTimerList::convertChannelId2String(const t_channel_id id) // UTF-8
{
	std::string name = CServiceManager::getInstance()->GetServiceName(id);
	if (name.empty())
		name = g_Locale->getText(LOCALE_TIMERLIST_PROGRAM_UNKNOWN);

	return name;
}

#define TIMERLIST_REPEAT_OPTION_COUNT 7
const CMenuOptionChooser::keyval TIMERLIST_REPEAT_OPTIONS[TIMERLIST_REPEAT_OPTION_COUNT] =
{
	{ CTimerd::TIMERREPEAT_ONCE,		LOCALE_TIMERLIST_REPEAT_ONCE },
	{ CTimerd::TIMERREPEAT_DAILY,		LOCALE_TIMERLIST_REPEAT_DAILY },
	{ CTimerd::TIMERREPEAT_WEEKLY,		LOCALE_TIMERLIST_REPEAT_WEEKLY },
	{ CTimerd::TIMERREPEAT_BIWEEKLY,	LOCALE_TIMERLIST_REPEAT_BIWEEKLY },
	{ CTimerd::TIMERREPEAT_FOURWEEKLY,	LOCALE_TIMERLIST_REPEAT_FOURWEEKLY },
	{ CTimerd::TIMERREPEAT_MONTHLY,		LOCALE_TIMERLIST_REPEAT_MONTHLY },
	{ CTimerd::TIMERREPEAT_WEEKDAYS,	LOCALE_TIMERLIST_REPEAT_WEEKDAYS }
};

#define TIMERLIST_STANDBY_OPTION_COUNT 2
const CMenuOptionChooser::keyval TIMERLIST_STANDBY_OPTIONS[TIMERLIST_STANDBY_OPTION_COUNT] =
{
	{ 0, LOCALE_TIMERLIST_STANDBY_OFF },
	{ 1, LOCALE_TIMERLIST_STANDBY_ON }
};

#if 1
#define TIMERLIST_TYPE_OPTION_COUNT 7
#else
#define TIMERLIST_TYPE_OPTION_COUNT 8
#endif
const CMenuOptionChooser::keyval TIMERLIST_TYPE_OPTIONS[TIMERLIST_TYPE_OPTION_COUNT] =
{
#if 0
	{ CTimerd::TIMER_NEXTPROGRAM,	LOCALE_TIMERLIST_TYPE_NEXTPROGRAM },
#endif
	{ CTimerd::TIMER_RECORD,	LOCALE_TIMERLIST_TYPE_RECORD },
	{ CTimerd::TIMER_ZAPTO,		LOCALE_TIMERLIST_TYPE_ZAPTO },
	{ CTimerd::TIMER_STANDBY,	LOCALE_TIMERLIST_TYPE_STANDBY },
	{ CTimerd::TIMER_SLEEPTIMER,	LOCALE_TIMERLIST_TYPE_SLEEPTIMER },
	{ CTimerd::TIMER_REMIND,	LOCALE_TIMERLIST_TYPE_REMIND },
	{ CTimerd::TIMER_SHUTDOWN,	LOCALE_TIMERLIST_TYPE_SHUTDOWN },
	{ CTimerd::TIMER_EXEC_PLUGIN,	LOCALE_TIMERLIST_TYPE_EXECPLUGIN }
};

#define MESSAGEBOX_NO_YES_OPTION_COUNT 2
const CMenuOptionChooser::keyval MESSAGEBOX_NO_YES_OPTIONS[MESSAGEBOX_NO_YES_OPTION_COUNT] =
{
	{ 0, LOCALE_MESSAGEBOX_NO },
	{ 1, LOCALE_MESSAGEBOX_YES }
};

int CTimerList::modifyTimer()
{
	CTimerd::responseGetTimer* timer=&timerlist[selected];
	CTimerd::responseGetTimer t_old = timerlist[selected];
	CMenuWidget timerSettings(LOCALE_TIMERLIST_MENUMODIFY, NEUTRINO_ICON_SETTINGS);
	timerSettings.addIntroItems();

	char type[80];
	strcpy(type, convertTimerType2String(timer->eventType)); // UTF
	CMenuForwarder *m0 = new CMenuForwarder(LOCALE_TIMERLIST_TYPE, false, type);
	timerSettings.addItem( m0);

	CDateInput timerSettings_alarmTime(LOCALE_TIMERLIST_ALARMTIME, &timer->alarmTime , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CMenuForwarder *m1 = new CMenuForwarder(LOCALE_TIMERLIST_ALARMTIME, true, timerSettings_alarmTime.getValue(), &timerSettings_alarmTime );
	timerSettings.addItem( m1);

	CDateInput timerSettings_stopTime(LOCALE_TIMERLIST_STOPTIME, &timer->stopTime , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	if (timer->stopTime != 0)
	{
		CMenuForwarder *m2 = new CMenuForwarder(LOCALE_TIMERLIST_STOPTIME, true, timerSettings_stopTime.getValue(), &timerSettings_stopTime );
		timerSettings.addItem( m2);
	}

	Timer->setWeekdaysToStr(timer->eventRepeat, m_weekdaysStr);
	timer->eventRepeat = (CTimerd::CTimerEventRepeat)(((int)timer->eventRepeat) & 0x1FF);
	CStringInput timerSettings_weekdays(LOCALE_TIMERLIST_WEEKDAYS, &m_weekdaysStr, 7, LOCALE_TIMERLIST_WEEKDAYS_HINT_1, LOCALE_TIMERLIST_WEEKDAYS_HINT_2, "-X");
	CMenuForwarder *m4 = new CMenuForwarder(LOCALE_TIMERLIST_WEEKDAYS, ((int)timer->eventRepeat) >= (int)CTimerd::TIMERREPEAT_WEEKDAYS, m_weekdaysStr, &timerSettings_weekdays );
	CIntInput timerSettings_repeatCount(LOCALE_TIMERLIST_REPEATCOUNT, (int*)&timer->repeatCount,3, LOCALE_TIMERLIST_REPEATCOUNT_HINT_1, LOCALE_TIMERLIST_REPEATCOUNT_HINT_2);

	CMenuForwarder *m5 = new CMenuForwarder(LOCALE_TIMERLIST_REPEATCOUNT, timer->eventRepeat != (int)CTimerd::TIMERREPEAT_ONCE ,timerSettings_repeatCount.getValue() , &timerSettings_repeatCount);

	CTimerListRepeatNotifier notifier((int *)&timer->eventRepeat,m4,m5, &m_weekdaysStr);
	CMenuOptionChooser* m3 = new CMenuOptionChooser(LOCALE_TIMERLIST_REPEAT, (int *)&timer->eventRepeat, TIMERLIST_REPEAT_OPTIONS, TIMERLIST_REPEAT_OPTION_COUNT, true, &notifier);

//printf("TIMER: rec dir %s len %s\n", timer->recordingDir, strlen(timer->recordingDir));

	timerSettings.addItem(GenericMenuSeparatorLine);
	timerSettings.addItem(m3);
	timerSettings.addItem(m4);
	timerSettings.addItem(m5);
	if (timer->eventType == CTimerd::TIMER_RECORD)
	{
		if (!strlen(timer->recordingDir))
			strncpy(timer->recordingDir,g_settings.network_nfs_recordingdir.c_str(),sizeof(timer->recordingDir)-1);
		timer_recordingDir = timer->recordingDir;

		bool recDirEnabled = (g_settings.recording_type == RECORDING_FILE); // obsolete?
		CMenuForwarder* m6 = new CMenuForwarder(LOCALE_TIMERLIST_RECORDING_DIR, recDirEnabled, timer_recordingDir, this, "rec_dir1", CRCInput::RC_green);
		timerSettings.addItem(GenericMenuSeparatorLine);
		timerSettings.addItem(m6);
	}

	CMenuWidget timerSettings_apids(LOCALE_TIMERLIST_APIDS, NEUTRINO_ICON_SETTINGS);
	CTimerListApidNotifier apid_notifier(&timer_apids_dflt, &timer_apids_std, &timer_apids_ac3, &timer_apids_alt);
	timer_apids_dflt = (timer->apids == 0) ? 1 : 0 ;
	timer_apids_std = (timer->apids & TIMERD_APIDS_STD) ? 1 : 0 ;
	timer_apids_ac3 = (timer->apids & TIMERD_APIDS_AC3) ? 1 : 0 ;
	timer_apids_alt = (timer->apids & TIMERD_APIDS_ALT) ? 1 : 0 ;
	int t_old_apids_dflt = timer_apids_dflt;
	int t_old_apids_std = timer_apids_std;
	int t_old_apids_ac3 = timer_apids_ac3;
	int t_old_apids_alt = timer_apids_alt;
	timerSettings_apids.addIntroItems();
	CMenuOptionChooser* ma1 = new CMenuOptionChooser(LOCALE_TIMERLIST_APIDS_DFLT, &timer_apids_dflt, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, &apid_notifier);
	timerSettings_apids.addItem(ma1);
	CMenuOptionChooser* ma2 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_STD, &timer_apids_std, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, !timer_apids_dflt, &apid_notifier);
	timerSettings_apids.addItem(ma2);
	CMenuOptionChooser* ma3 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_ALT, &timer_apids_alt, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, !timer_apids_dflt, &apid_notifier);
	timerSettings_apids.addItem(ma3);
	CMenuOptionChooser* ma4 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_AC3, &timer_apids_ac3, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, !timer_apids_dflt, &apid_notifier);
	timerSettings_apids.addItem(ma4);
	apid_notifier.setItems(ma1,ma2,ma3,ma4);
	if (timer->eventType == CTimerd::TIMER_RECORD)
	{
		timerSettings.addItem( new CMenuForwarder(LOCALE_TIMERLIST_APIDS, true, NULL, &timerSettings_apids ));
	}

	int res = timerSettings.exec(this,"");

	bool modified = (
		   timer->alarmTime   != t_old.alarmTime
		|| timer->stopTime    != t_old.stopTime
		|| timer->eventRepeat != t_old.eventRepeat
		|| timer->repeatCount != t_old.repeatCount
	);
	if (!modified && timer->eventType == CTimerd::TIMER_RECORD)
	{
		if (timer->recordingDir && t_old.recordingDir)
			modified = strcmp(timer->recordingDir, t_old.recordingDir);
		if (!modified)
			modified = (
				   t_old_apids_dflt != timer_apids_dflt
				|| t_old_apids_std  != timer_apids_std
				|| t_old_apids_ac3  != timer_apids_ac3
				|| t_old_apids_alt  != timer_apids_alt
			);
	}
	if (modified)
	{
		if (ShowMsg(LOCALE_TIMERLIST_SAVE, LOCALE_PERSONALIZE_APPLY_SETTINGS, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450, 20, true) == CMsgBox::mbrYes)
			exec(NULL, "modifytimer");
	}

	return res;
}

int CTimerList::newTimer()
{
	std::vector<CMenuWidget *> toDelete;
	// Defaults
	timerNew.eventType = CTimerd::TIMER_RECORD ;
	if (g_settings.recording_type == CNeutrinoApp::RECORDING_OFF)
		timerNew.eventType = CTimerd::TIMER_ZAPTO;

	timerNew.eventRepeat = CTimerd::TIMERREPEAT_ONCE ;
	timerNew.repeatCount = 0;
	timerNew.alarmTime = (time(NULL)/60)*60;
	timerNew.stopTime = (time(NULL)/60)*60;
	timerNew.channel_id = 0;
	strcpy(timerNew.message, "---");
	timerNew_standby_on =false;
	strncpy(timerNew.recordingDir,g_settings.network_nfs_recordingdir.c_str(),sizeof(timerNew.recordingDir)-1);

	CMenuWidget timerSettings(LOCALE_TIMERLIST_MENUNEW, NEUTRINO_ICON_SETTINGS);
	timerSettings.addIntroItems();
	timerSettings.addItem(new CMenuForwarder(LOCALE_TIMERLIST_SAVE, true, NULL, this, "newtimer", CRCInput::RC_red));
	timerSettings.addItem(GenericMenuSeparatorLine);

	CDateInput timerSettings_alarmTime(LOCALE_TIMERLIST_ALARMTIME, &(timerNew.alarmTime) , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CMenuForwarder *m1 = new CMenuForwarder(LOCALE_TIMERLIST_ALARMTIME, true, timerSettings_alarmTime.getValue(), &timerSettings_alarmTime );

	CDateInput timerSettings_stopTime(LOCALE_TIMERLIST_STOPTIME, &(timerNew.stopTime) , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CMenuForwarder *m2 = new CMenuForwarder(LOCALE_TIMERLIST_STOPTIME, true, timerSettings_stopTime.getValue(), &timerSettings_stopTime );

	CStringInput timerSettings_weekdays(LOCALE_TIMERLIST_WEEKDAYS, &m_weekdaysStr, 7, LOCALE_TIMERLIST_WEEKDAYS_HINT_1, LOCALE_TIMERLIST_WEEKDAYS_HINT_2, "-X");
	CMenuForwarder *m4 = new CMenuForwarder(LOCALE_TIMERLIST_WEEKDAYS, false, m_weekdaysStr, &timerSettings_weekdays);

	CIntInput timerSettings_repeatCount(LOCALE_TIMERLIST_REPEATCOUNT, (int*)&timerNew.repeatCount,3, LOCALE_TIMERLIST_REPEATCOUNT_HINT_1, LOCALE_TIMERLIST_REPEATCOUNT_HINT_2);
	CMenuForwarder *m5 = new CMenuForwarder(LOCALE_TIMERLIST_REPEATCOUNT, false,timerSettings_repeatCount.getValue() , &timerSettings_repeatCount);

	CTimerListRepeatNotifier notifier((int *)&timerNew.eventRepeat,m4,m5, &m_weekdaysStr);
	m_weekdaysStr = "XXXXX--";
	CMenuOptionChooser* m3 = new CMenuOptionChooser(LOCALE_TIMERLIST_REPEAT, (int *)&timerNew.eventRepeat, TIMERLIST_REPEAT_OPTIONS, TIMERLIST_REPEAT_OPTION_COUNT, true, &notifier);

	CMenuWidget mctv(LOCALE_TIMERLIST_BOUQUETSELECT, NEUTRINO_ICON_SETTINGS);
	CMenuWidget mcradio(LOCALE_TIMERLIST_BOUQUETSELECT, NEUTRINO_ICON_SETTINGS);

	for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++)
	{
		if (!g_bouquetManager->Bouquets[i]->bHidden)
		{
			CMenuWidget* mwtv = new CMenuWidget(LOCALE_TIMERLIST_CHANNELSELECT, NEUTRINO_ICON_SETTINGS);
			toDelete.push_back(mwtv);
			CMenuWidget* mwradio = new CMenuWidget(LOCALE_TIMERLIST_CHANNELSELECT, NEUTRINO_ICON_SETTINGS);
			toDelete.push_back(mwradio);

			ZapitChannelList channels;
			g_bouquetManager->Bouquets[i]->getTvChannels(channels);
			for (int j = 0; j < (int) channels.size(); j++)
			{
				char cChannelId[3+16+1+1];
				sprintf(cChannelId, "SC:" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS ",", channels[j]->getChannelID());
				mwtv->addItem(new CMenuForwarder(channels[j]->getName(), true, NULL, this, (std::string(cChannelId) + channels[j]->getName()).c_str(), CRCInput::RC_nokey, NULL, channels[j]->scrambled ? NEUTRINO_ICON_SCRAMBLED : (channels[j]->getUrl().empty() ? NULL : NEUTRINO_ICON_STREAMING)));
			}
			if (!channels.empty())
				mctv.addItem(new CMenuForwarder(g_bouquetManager->Bouquets[i]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : g_bouquetManager->Bouquets[i]->Name.c_str() /*g_bouquetManager->Bouquets[i]->Name.c_str()*/, true, NULL, mwtv));


			g_bouquetManager->Bouquets[i]->getRadioChannels(channels);
			for (int j = 0; j < (int) channels.size(); j++)
			{
				char cChannelId[3+16+1+1];
				sprintf(cChannelId, "SC:" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS ",", channels[j]->getChannelID());
				mwradio->addItem(new CMenuForwarder(channels[j]->getName(), true, NULL, this, (std::string(cChannelId) + channels[j]->getName()).c_str(), CRCInput::RC_nokey, NULL, channels[j]->scrambled ? NEUTRINO_ICON_SCRAMBLED : (channels[j]->getUrl().empty() ? NULL : NEUTRINO_ICON_STREAMING)));
			}
			if (!channels.empty())
				mcradio.addItem(new CMenuForwarder(g_bouquetManager->Bouquets[i]->Name.c_str(), true, NULL, mwradio));
		}
	}

	CMenuWidget mm(LOCALE_TIMERLIST_MODESELECT, NEUTRINO_ICON_SETTINGS);
	mm.addItem(new CMenuForwarder(LOCALE_TIMERLIST_MODETV, true, NULL, &mctv));
	mm.addItem(new CMenuForwarder(LOCALE_TIMERLIST_MODERADIO, true, NULL, &mcradio));
	timerNew_channel_name = "---";
	CMenuForwarder* m6 = new CMenuForwarder(LOCALE_TIMERLIST_CHANNEL, true, timerNew_channel_name, &mm);
	timerNew_recordingDir = timerNew.recordingDir;
	CMenuForwarder* m7 = new CMenuForwarder(LOCALE_TIMERLIST_RECORDING_DIR, true, timerNew_recordingDir, this, "rec_dir2", CRCInput::RC_green);

	CMenuOptionChooser* m8 = new CMenuOptionChooser(LOCALE_TIMERLIST_STANDBY, &timerNew_standby_on, TIMERLIST_STANDBY_OPTIONS, TIMERLIST_STANDBY_OPTION_COUNT, false);

	timerNew_message = std::string(timerNew.message);
	CKeyboardInput timerSettings_msg(LOCALE_TIMERLIST_MESSAGE, &timerNew_message);
	CMenuForwarder *m9 = new CMenuForwarder(LOCALE_TIMERLIST_MESSAGE, false, timerNew_message, &timerSettings_msg );

	timerNew_pluginName = "---";
	CPluginChooser plugin_chooser(LOCALE_TIMERLIST_PLUGIN, CPlugins::P_TYPE_SCRIPT | CPlugins::P_TYPE_TOOL
										       | CPlugins::P_TYPE_LUA
										       , timerNew_pluginName);
	CMenuForwarder *m10 = new CMenuForwarder(LOCALE_TIMERLIST_PLUGIN, false, timerNew_pluginName, &plugin_chooser);


	CTimerListNewNotifier notifier2((int *)&timerNew.eventType,
					&timerNew.stopTime,m2,m6,m8,m9,m10,m7,
					&timerSettings_stopTime.getValue());
	CMenuOptionChooser* m0;
	if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF)
		m0 = new CMenuOptionChooser(LOCALE_TIMERLIST_TYPE, (int *)&timerNew.eventType, TIMERLIST_TYPE_OPTIONS, TIMERLIST_TYPE_OPTION_COUNT, true, &notifier2, CRCInput::RC_nokey, "", true, true);
	else
		m0 = new CMenuOptionChooser(LOCALE_TIMERLIST_TYPE, (int *)&timerNew.eventType, &TIMERLIST_TYPE_OPTIONS[1], TIMERLIST_TYPE_OPTION_COUNT-1, true, &notifier2);

	timerSettings.addItem( m0);
	timerSettings.addItem( m1);
	timerSettings.addItem( m2);
	timerSettings.addItem(GenericMenuSeparatorLine);
	timerSettings.addItem( m3);
	timerSettings.addItem( m4);
	timerSettings.addItem( m5);
	timerSettings.addItem(GenericMenuSeparatorLine);
	timerSettings.addItem( m6);
	timerSettings.addItem( m7);
	timerSettings.addItem( m8);
	timerSettings.addItem( m9);
	timerSettings.addItem( m10);

	notifier2.changeNotify(NONEXISTANT_LOCALE, NULL);
	int ret=timerSettings.exec(this,"");

	cstrncpy(timerNew.pluginName, timerNew_pluginName, sizeof(timerNew.pluginName));
	cstrncpy(timerNew.message, timerNew_message, sizeof(timerNew.message));

	// delete dynamic created objects
	for (unsigned int count=0; count<toDelete.size(); count++)
	{
		delete toDelete[count];
	}
	toDelete.clear();

	return ret;
}

bool CTimerList::askUserOnRemoteTimerConflict(time_t announceTime, time_t stopTime, char * remotebox_name)
{
	CTimerd::TimerList overlappingTimers;
	int pre,post;
	Timer->getRecordingSafety(pre,post);

	for (CTimerd::TimerList::iterator it = timerlist.begin(); it != timerlist.end();++it)
	{
		if (strcmp(it->remotebox_name,remotebox_name) == 0)
		{
			if (it->stopTime != 0 && stopTime != 0)
			{
				// Check if both timers have start and end. In this case do not show conflict, if endtime is the same than the starttime of the following timer
				if ((stopTime+post > it->alarmTime) && (announceTime-pre < it->stopTime))
				{
					overlappingTimers.push_back(*it);
				}
			}
			else
			{
				if (!((stopTime < it->announceTime) || (announceTime > it->stopTime)))
				{
					overlappingTimers.push_back(*it);
				}
			}
		}
	}

	std::string timerbuf = g_Locale->getText(LOCALE_TIMERLIST_OVERLAPPING_TIMER);
	timerbuf += "\n";
	for (CTimerd::TimerList::iterator it = overlappingTimers.begin(); it != overlappingTimers.end(); ++it)
	{
		timerbuf += CTimerList::convertTimerType2String(it->eventType);
		timerbuf += " (";
		timerbuf += CTimerList::convertChannelId2String(it->channel_id); // UTF-8
		if (it->epgID != 0)
		{
			CEPGData epgdata;
			if (CEitManager::getInstance()->getEPGid(it->epgID, it->epg_starttime, &epgdata))
			{
				timerbuf += ":";
				timerbuf += epgdata.title;
			}
			else if (strlen(it->epgTitle)!=0)
			{
				timerbuf += ":";
				timerbuf += it->epgTitle;
			}
		}
		timerbuf += "):\n";

		struct tm *annTime = localtime(&(it->announceTime));
		timerbuf += strftime("%d.%m. %H:%M\n",annTime);

		struct tm *sTime = localtime(&(it->stopTime));
		timerbuf += strftime("%d.%m. %H:%M\n",sTime);
	}
	if (overlappingTimers.size() > 0)
		return (ShowMsg(LOCALE_MESSAGEBOX_INFO,timerbuf,CMsgBox::mbrNo,CMsgBox::mbNo|CMsgBox::mbYes) == CMsgBox::mbrYes);
	else
		return true;
}

bool askUserOnTimerConflict(time_t announceTime, time_t stopTime, t_channel_id channel_id)
{
	if (CFEManager::getInstance()->getEnabledCount() == 1) {
		CTimerdClient Timer;
		CTimerd::TimerList overlappingTimers = Timer.getOverlappingTimers(announceTime,stopTime);
		//printf("[CTimerdClient] attention\n%d\t%d\t%d conflicts with:\n",timerNew.announceTime,timerNew.alarmTime,timerNew.stopTime);

		// Don't ask if there are overlapping timers on the same transponder.
		if (channel_id) {
			CTimerd::TimerList::iterator i;
			for (i = overlappingTimers.begin(); i != overlappingTimers.end(); i++)
				if ((i->eventType != CTimerd::TIMER_RECORD || !SAME_TRANSPONDER(channel_id, i->channel_id)))
					break;
			if (i == overlappingTimers.end())
				return true; // yes, add timer
		}

		std::string timerbuf = g_Locale->getText(LOCALE_TIMERLIST_OVERLAPPING_TIMER);
		timerbuf += "\n";
		for (CTimerd::TimerList::iterator it = overlappingTimers.begin(); it != overlappingTimers.end(); ++it)
		{
			timerbuf += CTimerList::convertTimerType2String(it->eventType);
			timerbuf += " (";
			timerbuf += CTimerList::convertChannelId2String(it->channel_id); // UTF-8
			if (it->epgID != 0)
			{
				CEPGData epgdata;
				if (CEitManager::getInstance()->getEPGid(it->epgID, it->epg_starttime, &epgdata))
				{
					timerbuf += ":";
					timerbuf += epgdata.title;
				}
				else if (strlen(it->epgTitle)!=0)
				{
					timerbuf += ":";
					timerbuf += it->epgTitle;
				}
			}
			timerbuf += "):\n";

			struct tm *annTime = localtime(&(it->announceTime));
			timerbuf += strftime("%d.%m. %H:%M\n",annTime);

			struct tm *sTime = localtime(&(it->stopTime));
			timerbuf += strftime("%d.%m. %H:%M\n",sTime);
			//printf("%d\t%d\t%d\n",it->announceTime,it->alarmTime,it->stopTime);
		}
		//printf("message:\n%s\n",timerbuf.c_str());
		// todo: localize message
		//g_Locale->getText(TIMERLIST_OVERLAPPING_MESSAGE);

		return (ShowMsg(LOCALE_MESSAGEBOX_INFO,timerbuf,CMsgBox::mbrNo,CMsgBox::mbNo|CMsgBox::mbYes) == CMsgBox::mbrYes);
	}
	else
		return true;
}
