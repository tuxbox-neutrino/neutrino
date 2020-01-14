/*
	Neutrino-GUI  -   DBoxII-Project

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <global.h>
#include <neutrino.h>
#include <driver/rcinput.h>
#include <gui/motorcontrol.h>
#include <gui/scan_setup.h>
#include <gui/color.h>
#include <gui/widget/menue.h>
#include <gui/widget/msgbox.h>
#include <system/settings.h>
#include <driver/screen_max.h>
#include <driver/neutrinofonts.h>
#include <driver/fontrenderer.h>
#include <zapit/satconfig.h>
#include <zapit/zapit.h>
#include <zapit/scannit.h>

#define BAR_BORDER 2
#define BAR_WIDTH 100
#define BAR_HEIGHT 16 //(13 + BAR_BORDER*2)

CMotorControl::CMotorControl(int tnum)
{
	printf("CMotorControl::CMotorControl: tuner %d\n", tnum);
	frontend = CFEManager::getInstance()->getFE(tnum);
	if(frontend == NULL) {
		printf("CMotorControl::CMotorControl: BUG, invalid tuner number %d, using first\n", tnum);
		frontend = CFEManager::getInstance()->getFE(0);
	}
	Init();
}

CMotorControl::~CMotorControl()
{
	printf("CMotorControl::~CMotorControl\n");
}

void CMotorControl::Init(void)
{
	frameBuffer = CFrameBuffer::getInstance();
	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = 30;

	satfindpid = -1;

	width = w_max(720, 0);
	m_font  = *CNeutrinoFonts::getInstance()->getDynFont(width, mheight);
	height = hheight + (24 * mheight) - 5;
	height = h_max(height, 0);

	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;

	stepSize = 1; //default: 1 step
	stepMode = STEP_MODE_ON;
	installerMenue = false;
	motorPosition = 1;
	satellitePosition = 0;
	stepDelay = 10;
	signalbox = NULL;
	rotor_swap = frontend->getRotorSwap();
}

int CMotorControl::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	TP_params TP;
	int wasgrow = 0;
	uint8_t origPosition;

	last_snr = moving = g_sig = g_snr = 0;
	network = "unknown";

	int lim_cmd;
	int retval = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;

	/* send satellite list to zapit */
	CZapitClient::ScanSatelliteList satList;
	CZapitClient::commandSetScanSatelliteList sat;

	sat.position = CServiceManager::getInstance()->GetSatellitePosition(scansettings.satName);
	strncpy(sat.satName, scansettings.satName.c_str(), sizeof(sat.satName)-1);
	satList.push_back(sat);

	satellite_map_t & satmap = frontend->getSatellites();
	sat_iterator_t sit = satmap.find(sat.position);
	if(sit != satmap.end() && sit->second.motor_position)
		motorPosition = sit->second.motor_position;

	origPosition = motorPosition;
	//FIXME change cZapit live fe
	g_Zapit->stopPlayBack();
	g_Zapit->setScanSatelliteList(satList);
	CZapit::getInstance()->SetLiveFrontend(frontend);

	TP.feparams.frequency = atoi(scansettings.sat_TP_freq.c_str());
	TP.feparams.symbol_rate = atoi(scansettings.sat_TP_rate.c_str());
	TP.feparams.fec_inner = (fe_code_rate_t)scansettings.sat_TP_fec;
	TP.feparams.polarization = scansettings.sat_TP_pol;
	TP.feparams.delsys = (delivery_system_t) scansettings.sat_TP_delsys;
	TP.feparams.modulation = (fe_modulation_t) scansettings.sat_TP_mod;
	TP.TP_id = 0;
	g_Zapit->tune_TP(TP);

	paintHead();
	paintMenu();
	paintStatus();

	msg = CRCInput::RC_nokey;
	while (!(msg == CRCInput::RC_setup) && (!(msg == CRCInput::RC_home)))
	{
		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS(100 /*250*/);
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
		showSNR();
		//printf("SIG: %d SNR %d last %d\n", g_sig, g_snr, last_snr);
		if(moving && (stepMode == STEP_MODE_AUTO)) {
			if(last_snr < g_snr) {
				wasgrow = 1;
			}
			//if((last_snr > g_snr) && last_snr > 37000)
			if(wasgrow && (last_snr > g_snr) /* && last_snr > 50*/) {
				//printf("Must stop rotor!!!\n");
				g_Zapit->sendMotorCommand(0xE0, 0x31, 0x60, 0, 0, 0);
				moving = 0;
				paintStatus();
				last_snr = 0;
				//g_Zapit->tune_TP(TP);
			} else
				last_snr = g_snr;
		} else
			wasgrow = 0;

		if (msg == (neutrino_msg_t)g_settings.key_volumeup)
			msg = CRCInput::RC_plus;
		else if (msg == (neutrino_msg_t)g_settings.key_volumedown)
			msg = CRCInput::RC_minus;

		if (msg == CRCInput::RC_ok || msg == CRCInput::RC_0) {
			printf("[motorcontrol] 0 key received... goto %s\n", installerMenue ? "userMenue" : "installerMenue");
			if (installerMenue)
				installerMenue = false;
			else
				installerMenue = true;
			paintMenu();
			paintStatus();
		}
		else if (msg == CRCInput::RC_1 || msg == CRCInput::RC_left) {
			printf("[motorcontrol] left/1 key received... drive/Step motor west, stepMode: %d\n", stepMode);
			motorStep(true);
			paintStatus();
			//g_Zapit->tune_TP(TP);
		}
		else if (msg == CRCInput::RC_red || msg == CRCInput::RC_2) {
			printf("[motorcontrol] 2 key received... halt motor\n");
			g_Zapit->sendMotorCommand(0xE0, 0x31, 0x60, 0, 0, 0);
			moving = 0;
			paintStatus();
			//g_Zapit->tune_TP(TP);
		}
		else if (msg == CRCInput::RC_3 || msg == CRCInput::RC_right) {
			printf("[motorcontrol] right/3 key received... drive/Step motor east, stepMode: %d\n", stepMode);
			motorStep(false);
			paintStatus();
			//g_Zapit->tune_TP(TP);
		}
		else if (msg == CRCInput::RC_4) {
			if (installerMenue) {
				printf("[motorcontrol] 4 key received... set west (soft) limit\n");
				if(rotor_swap) lim_cmd = 0x66;
				else lim_cmd = 0x67;
				g_Zapit->sendMotorCommand(0xE1, 0x31, lim_cmd, 0, 0, 0);
				//g_Zapit->tune_TP(TP);
			}
		}
		else if (msg == CRCInput::RC_5 || msg == CRCInput::RC_green) {
			if (installerMenue) {
				printf("[motorcontrol] 5 key received... disable (soft) limits\n");
				g_Zapit->sendMotorCommand(0xE0, 0x31, 0x63, 0, 0, 0);
			} else {
				bool store = true;
				printf("[motorcontrol] 5 key received... store present satellite number: %d\n", motorPosition);
				if(motorPosition != origPosition) {
					printf("[motorcontrol] position changed %d -> %d\n", origPosition, motorPosition);
					for(sit = satmap.begin(); sit != satmap.end(); sit++) {
						if(sit->second.motor_position == motorPosition)
							break;
					}
					if(sit != satmap.end()) {
						std::string satname = CServiceManager::getInstance()->GetSatelliteName(sit->first);
						printf("[motorcontrol] new positions configured for %s\n", satname.c_str());
						std::string buf = g_Locale->getText(LOCALE_MOTORCONTROL_OVERRIDE);
						buf += " ";
						buf += satname;
						buf += " ?";
						store = (ShowMsg(LOCALE_MESSAGEBOX_INFO, buf,CMsgBox::mbrNo,CMsgBox::mbNo|CMsgBox::mbYes) == CMsgBox::mbrYes);
					}
				}
				if(store)
					g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6A, 1, motorPosition, 0);
			}
		}
		else if (msg == CRCInput::RC_6) {
			if (installerMenue) {
				printf("[motorcontrol] 6 key received... set east (soft) limit\n");
				if(rotor_swap) lim_cmd = 0x67;
				else lim_cmd = 0x66;
				g_Zapit->sendMotorCommand(0xE1, 0x31, lim_cmd, 0, 0, 0);
				//g_Zapit->tune_TP(TP);
			} else {
				if (stepSize < 0x7F) stepSize++;
				printf("[motorcontrol] 6 key received... increase Step size: %d\n", stepSize);
				paintStatus();
			}
		}
		else if (msg == CRCInput::RC_7 || msg == CRCInput::RC_yellow) {
			if (installerMenue) {
				printf("[motorcontrol] 7 key received... goto reference position\n");
				g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6B, 1, 0, 0);
				satellitePosition = 0;
				paintStatus();
			} else {
				printf("[motorcontrol] 7 key received... goto satellite number: %d\n", motorPosition);
				g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6B, 1, motorPosition, 0);
				satellitePosition = 0;
				paintStatus();
			}
			//g_Zapit->tune_TP(TP);
		}
		else if (msg == CRCInput::RC_8) {
			if (installerMenue) {
				printf("[motorcontrol] 8 key received... enable (soft) limits\n");
				g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6A, 1, 0, 0);
			}
		}
		else if (msg == CRCInput::RC_9) {
			if (installerMenue) {
				printf("[motorcontrol] 9 key received... (re)-calculate positions\n");
				g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6F, 1, 0, 0);
			} else {
				if (stepSize > 1) stepSize--;
				printf("[motorcontrol] 9 key received... decrease Step size: %d\n", stepSize);
				paintStatus();
			}
		}
		else if (msg == CRCInput::RC_plus || msg == CRCInput::RC_up) {
			printf("[motorcontrol] up key received... increase satellite position: %d\n", ++motorPosition);
			satellitePosition = 0;
			paintStatus();
		}
		else if (msg == CRCInput::RC_minus || msg == CRCInput::RC_down) {
			if (motorPosition > 1) motorPosition--;
			printf("[motorcontrol] down key received... decrease satellite position: %d\n", motorPosition);
			satellitePosition = 0;
			paintStatus();
		}
		else if (msg == CRCInput::RC_blue) {
			if (++stepMode > 3)
				stepMode = 0;
			if (stepMode == STEP_MODE_OFF)
				satellitePosition = 0;
			last_snr = 0;
			printf("[motorcontrol] blue key received... toggle stepmode on/off: %d\n", stepMode);
			paintStatus();
		}
		else if (msg == CRCInput::RC_info || msg == CRCInput::RC_help) {
			network = "waiting for NIT...";
			paintStatus();
			readNetwork();
			paintStatus();
		}
		else if (msg == CRCInput::RC_setup) {
			retval = menu_return::RETURN_EXIT_ALL;
		}
		else {
			if ((msg >= CRCInput::RC_WithData) && (msg < CRCInput::RC_WithData + 0x10000000))
				delete[] (unsigned char*) data;
		}
	}

	delete signalbox;
	signalbox = NULL;
	hide();
	frontend->setTsidOnid(0);

	return retval;
}

void CMotorControl::motorStep(bool west)
{
	int cmd;
	if (west) {
		if(rotor_swap) cmd = 0x68;
		else cmd = 0x69;
	} else {
		if(rotor_swap) cmd = 0x69;
		else cmd = 0x68;
	}
	printf("[motorcontrol] motorStep: %s\n", west ? "West" : "East");
	switch(stepMode)
	{
		case STEP_MODE_ON:
			g_Zapit->sendMotorCommand(0xE0, 0x31, cmd, 1, (-1 * stepSize), 0);
			satellitePosition += west ? stepSize : -stepSize;
			break;
		case STEP_MODE_TIMED:
			g_Zapit->sendMotorCommand(0xE0, 0x31, cmd, 1, 40, 0);
			usleep(stepSize * stepDelay * 1000);
			g_Zapit->sendMotorCommand(0xE0, 0x31, 0x60, 0, 0, 0); //halt motor
			satellitePosition += west ? stepSize : -stepSize;
			break;
		case STEP_MODE_AUTO:
			moving = 1;
			paintStatus();
			/* fall through */
		default:
			/* what is STEP_MODE_OFF supposed to do? */
			g_Zapit->sendMotorCommand(0xE0, 0x31, cmd, 1, 40, 0);
	}
}

void CMotorControl::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width, height + 20);
	stopSatFind();
}

void CMotorControl::paintLine(int px, int *py, int pwidth, const char *txt)
{
	frameBuffer->paintBoxRel(px, *py, pwidth, mheight, COL_MENUCONTENT_PLUS_0);
	*py += mheight;
	m_font->RenderString(px, *py, pwidth, txt, COL_MENUCONTENT_TEXT);
}

void CMotorControl::paintLine(int px, int py, int pwidth, const char *txt)
{
	m_font->RenderString(px, py, pwidth, txt, COL_MENUCONTENT_TEXT);
}

void CMotorControl::paintLine(int ix, int tx, int *py, int pwidth, const char *icon, const char *txt)
{
	frameBuffer->paintIcon(icon, ix, *py, mheight);
	*py += mheight;
	m_font->RenderString(tx, *py, pwidth, txt, COL_MENUCONTENT_TEXT);
}

void CMotorControl::paintSeparator(int xpos, int *pypos, int pwidth, const char * /*txt*/)
{
	int th = 10;
	//*ypos += mheight;
	*pypos += th;
	frameBuffer->paintHLineRel(xpos, pwidth - 20, *pypos - (th >> 1), COL_MENUCONTENT_PLUS_3);
	frameBuffer->paintHLineRel(xpos, pwidth - 20, *pypos - (th >> 1) + 1, COL_MENUCONTENT_PLUS_1);

#if 0
	int stringwidth = m_font->getRenderWidth(txt);
	int stringstartposX = 0;
	stringstartposX = (xpos + (pwidth >> 1)) - (stringwidth >> 1);
	frameBuffer->paintBoxRel(stringstartposX - 5, *pypos - mheight, stringwidth + 10, mheight, COL_MENUCONTENT_PLUS_0);
	m_font->RenderString(stringstartposX, *pypos, stringwidth, txt, COL_MENUCONTENT_TEXT);
#endif
}

void CMotorControl::paintStatus()
{
	char buf1[256];
	char buf2[256];

	int xpos1 = x + 10;
	int xpos2 = xpos1 + 10 + m_font->getRenderWidth(g_Locale->getText(LOCALE_MOTORCONTROL_MOTOR_POS));
	int width2 = width - (xpos2 - xpos1) - 10;
	int width1 = width - 10;

	ypos = ypos_status;
	paintSeparator(xpos1, &ypos, width, g_Locale->getText(LOCALE_MOTORCONTROL_SETTINGS));

	paintLine(xpos1, &ypos, width1, g_Locale->getText(LOCALE_MOTORCONTROL_MOTOR_POS));
	sprintf(buf1, "%d", motorPosition);
	paintLine(xpos2, ypos, width2 , buf1);

	paintLine(xpos1, &ypos, width1, g_Locale->getText(LOCALE_MOTORCONTROL_MOVEMENT));
	switch(stepMode)
	{
		case STEP_MODE_ON:
			strcpy(buf1, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_MODE));
			sprintf(buf2, "%d", stepSize);
			break;
		case STEP_MODE_AUTO:
			strcpy(buf1, g_Locale->getText(LOCALE_MOTORCONTROL_DRIVE_MODE_AUTO));
			if(moving)
				strcpy(buf2, g_Locale->getText(LOCALE_MOTORCONTROL_STOP_MOVING));
			else
				strcpy(buf2, g_Locale->getText(LOCALE_MOTORCONTROL_STOP_STOPPED));
			break;
		case STEP_MODE_OFF:
			strcpy(buf1, g_Locale->getText(LOCALE_MOTORCONTROL_DRIVE_MODE));
			strcpy(buf2, g_Locale->getText(LOCALE_MOTORCONTROL_NO_MODE));
			break;
		case STEP_MODE_TIMED:
			strcpy(buf1, g_Locale->getText(LOCALE_MOTORCONTROL_TIMED_MODE));
			sprintf(buf2, "%d %s", stepSize * stepDelay, g_Locale->getText(LOCALE_MOTORCONTROL_MSEC));
			break;
	}
	paintLine(xpos2, ypos, width2, buf1);

	paintLine(xpos1, &ypos, width1, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_SIZE));
	paintLine(xpos2, ypos, width2, buf2);

	paintLine(xpos1, &ypos, width1, g_Locale->getText(LOCALE_MOTORCONTROL_NETWORK));
	paintLine(xpos2, ypos, width2, network.c_str());

	paintSeparator(xpos1, &ypos, width, g_Locale->getText(LOCALE_MOTORCONTROL_STATUS));

	sprintf(buf1, "%s %d", g_Locale->getText(LOCALE_MOTORCONTROL_SAT_POS), satellitePosition);
	paintLine(xpos1, &ypos, width1, buf1);

	paintSeparator(xpos1, &ypos, width, g_Locale->getText(LOCALE_MOTORCONTROL_SETTINGS));
}

void CMotorControl::paintHead()
{
	CComponentsHeader header(x, y, width, hheight, LOCALE_MOTORCONTROL_HEAD);
	header.paint(CC_SAVE_SCREEN_NO);
}

void CMotorControl::paintMenu()
{
	frameBuffer->paintBoxRel(x, y + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	ypos = y + hheight + (mheight >> 1) - 10;

	int xpos1 = x + 10;
	int xpos2 = xpos1 + 10 + m_font->getRenderWidth("(7/yellow)");
	int width2 = width - (xpos2 - xpos1) - 10;

#if 1
	int width1 = width - 10;
	paintLine(xpos1, &ypos, width1, "(0/OK)");
	if(installerMenue)
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_USER_MENU));
	else
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_INSTALL_MENU));

	paintLine(xpos1, &ypos, width1, "(1/right)");
	paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_WEST));
	paintLine(xpos1, &ypos, width1, "(2/red)");
	paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_HALT));
	paintLine(xpos1, &ypos, width1, "(3/left)");
	paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_EAST));

	if (installerMenue)
	{
		paintLine(xpos1, &ypos, width1, "(4)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_WEST_LIMIT));
		paintLine(xpos1, &ypos, width1, "(5/green)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_DISABLE_LIMIT));
		paintLine(xpos1, &ypos, width1, "(6)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_EAST_LIMIT));
		paintLine(xpos1, &ypos, width1, "(7/yellow)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_REF_POSITION));
		paintLine(xpos1, &ypos, width1, "(8)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_ENABLE_LIMIT));
		paintLine(xpos1, &ypos, width1, "(9)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_CALC_POSITIONS));
	}
	else
	{
		paintLine(xpos1, &ypos, width1, "(4)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_NOTDEF));
		paintLine(xpos1, &ypos, width1, "(5/green)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_STORE));
		paintLine(xpos1, &ypos, width1, "(6)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_INCREASE));
		paintLine(xpos1, &ypos, width1, "(7/yellow)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_GOTO));
		paintLine(xpos1, &ypos, width1, "(8)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_NOTDEF));
		paintLine(xpos1, &ypos, width1, "(9)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_DECREASE));
	}
	paintLine(xpos1, &ypos, width1, "(+/up)");
	paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_POS_INCREASE));
	paintLine(xpos1, &ypos, width1, "(-/down)");
	paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_POS_DECREASE));
	paintLine(xpos1, &ypos, width1, "(blue)");
	paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_DRIVE));
	paintLine(xpos1, &ypos, width1, "(info/help)");
	paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_NETWORK));
#else
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_0,
			installerMenue ? g_Locale->getText(LOCALE_MOTORCONTROL_USER_MENU) : g_Locale->getText(LOCALE_MOTORCONTROL_INSTALL_MENU));

	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_1, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_WEST));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_2, g_Locale->getText(LOCALE_MOTORCONTROL_HALT));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_3, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_EAST));

	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_4,
			installerMenue ? g_Locale->getText(LOCALE_MOTORCONTROL_WEST_LIMIT) : g_Locale->getText(LOCALE_MOTORCONTROL_NOTDEF));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_5,
			installerMenue ? g_Locale->getText(LOCALE_MOTORCONTROL_DISABLE_LIMIT) : g_Locale->getText(LOCALE_MOTORCONTROL_STORE));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_6,
			installerMenue ? g_Locale->getText(LOCALE_MOTORCONTROL_EAST_LIMIT) : g_Locale->getText(LOCALE_MOTORCONTROL_STEP_INCREASE));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_7,
			installerMenue ? g_Locale->getText(LOCALE_MOTORCONTROL_REF_POSITION) : g_Locale->getText(LOCALE_MOTORCONTROL_GOTO));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_8,
			installerMenue ? g_Locale->getText(LOCALE_MOTORCONTROL_ENABLE_LIMIT) : g_Locale->getText(LOCALE_MOTORCONTROL_NOTDEF));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_9,
			installerMenue ? g_Locale->getText(LOCALE_MOTORCONTROL_CALC_POSITIONS) : g_Locale->getText(LOCALE_MOTORCONTROL_STEP_DECREASE));

	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_UP, g_Locale->getText(LOCALE_MOTORCONTROL_POS_INCREASE));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_DOWN, g_Locale->getText(LOCALE_MOTORCONTROL_POS_DECREASE));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_BLUE, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_DRIVE));
	paintLine(xpos1, xpos2, &ypos, width2, NEUTRINO_ICON_BUTTON_INFO_SMALL, g_Locale->getText(LOCALE_MOTORCONTROL_NETWORK));
#endif
	ypos_status = ypos;
}

void CMotorControl::startSatFind(void)
{
#if 0
	if (satfindpid != -1) {
		kill(satfindpid, SIGKILL);
		waitpid(satfindpid, 0, 0);
		satfindpid = -1;
	}

	switch ((satfindpid = fork())) {
		case -1:
			printf("[motorcontrol] fork");
			break;
		case 0:
			printf("[motorcontrol] starting satfind...\n");
			if (execlp("/bin/satfind", "satfind", NULL) < 0)
				printf("[motorcontrol] execlp satfind failed.\n");
			break;
	} /* switch */
#endif
}

void CMotorControl::stopSatFind(void)
{
#if 0
	if (satfindpid != -1) {
		printf("[motorcontrol] killing satfind...\n");
		kill(satfindpid, SIGKILL);
		waitpid(satfindpid, 0, 0);
		satfindpid = -1;
	}
#endif
}

void CMotorControl::showSNR ()
{
	if (signalbox == NULL){
		int xpos1 = x + 10;
		//signalbox = new CSignalBox(xpos1, y + height - mheight - 5, width - 2*(xpos1-x), m_font->getHeight(), frontend, false);
		signalbox = new CSignalBox(xpos1, y + height - (mheight*2*3)/2 - 5, width - 2*(xpos1-x), (m_font->getHeight()*2*3)/2, frontend, true);
		signalbox->setColorBody(COL_MENUCONTENT_PLUS_0);
		signalbox->setTextColor(COL_MENUCONTENT_TEXT);
		signalbox->doPaintBg(true);
	}

	signalbox->paint(false);
	g_snr = signalbox->getSNRValue();
}

void CMotorControl::readNetwork()
{
	CNit nit(0, 0, 0);
	nit.Start();
	nit.Stop();
	network = nit.GetNetworkName();
	t_satellite_position pos = nit.getOrbitalPosition();
	if (network.empty())
		network = "unknown";

	char net[100];
	snprintf(net, sizeof(net), "%03d.%d, %s", abs((int)pos)/10, abs((int)pos)%10, network.c_str());
	network = net;
}
