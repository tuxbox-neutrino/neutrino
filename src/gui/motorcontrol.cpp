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
#include <gui/customcolor.h>
#include <driver/rcinput.h>
#include <gui/motorcontrol.h>
#include <gui/scan_setup.h>
#include <gui/color.h>
#include <gui/components/cc_frm.h>
#include <gui/widget/menue.h>
#include <gui/widget/messagebox.h>
#include <system/settings.h>
#include <driver/screen_max.h>

#include <zapit/satconfig.h>
#include <zapit/zapit.h>

static int g_sig;
static int g_snr;
static int last_snr = 0;
static int moving = 0;

#define RED_BAR 40
#define YELLOW_BAR 70
#define GREEN_BAR 100

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
	delete sigscale;
	delete snrscale;
}

void CMotorControl::Init(void)
{
	frameBuffer = CFrameBuffer::getInstance();
	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();

	satfindpid = -1;

	width = w_max(470, 0);
	mheight = mheight - 2;
	height = hheight + (20 * mheight) - 5;
	height = h_max(height, 0);

	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;

	stepSize = 1; //default: 1 step
	stepMode = STEP_MODE_ON;
	installerMenue = false;
	motorPosition = 1;
	satellitePosition = 0;
	stepDelay = 10;
	sigscale = new CProgressBar(/*true, BAR_WIDTH, BAR_HEIGHT*/);
	sigscale->setBlink();
	snrscale = new CProgressBar(/*true, BAR_WIDTH, BAR_HEIGHT*/);
	snrscale->setBlink();
}

int CMotorControl::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	TP_params TP;
	int wasgrow = 0;
	uint8_t origPosition;

	last_snr = 0;
	moving = 0;

        CZapitClient::ScanSatelliteList satList;
        CZapitClient::commandSetScanSatelliteList sat;
	sat_iterator_t sit;

	sigscale->reset();
	snrscale->reset();
	lastsnr = lastsig = -1;

	bool istheend = false;
	int lim_cmd;
	if (!frameBuffer->getActive())
		return menu_return::RETURN_EXIT_ALL;

	if (parent)
		parent->hide();

        x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
        y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;

#if 0
       	/* send satellite list to zapit */
	for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
		if(!strcmp(sit->second.name.c_str(),scansettings.satName)) {
			sat.position = sit->first;
			strncpy(sat.satName, scansettings.satName, 49);
			satList.push_back(sat);
			if(sit->second.motor_position)
				motorPosition = sit->second.motor_position;
			break;
		}
	}
#endif
       	/* send satellite list to zapit */
	sat.position = CServiceManager::getInstance()->GetSatellitePosition(scansettings.satName);
	strncpy(sat.satName, scansettings.satName, 49);
	satList.push_back(sat);

	satellite_map_t & satmap = frontend->getSatellites();
	sit = satmap.find(sat.position);
	if(sit != satmap.end() && sit->second.motor_position)
		motorPosition = sit->second.motor_position;

	origPosition = motorPosition;
	//FIXME change cZapit live fe
	g_Zapit->stopPlayBack();
       	g_Zapit->setScanSatelliteList( satList);
	CZapit::getInstance()->SetLiveFrontend(frontend);

	TP.feparams.dvb_feparams.frequency = atoi(scansettings.sat_TP_freq);
	TP.feparams.dvb_feparams.u.qpsk.symbol_rate = atoi(scansettings.sat_TP_rate);
	TP.feparams.dvb_feparams.u.qpsk.fec_inner = (fe_code_rate_t)scansettings.sat_TP_fec;
	TP.polarization = scansettings.sat_TP_pol;

	g_Zapit->tune_TP(TP);

	paintHead();
	paintMenu();
	paintStatus();
	frameBuffer->blit();

	while (!istheend)
	{

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS(100 /*250*/);
		msg = CRCInput::RC_nokey;

		while (!(msg == CRCInput::RC_timeout) && (!(msg == CRCInput::RC_home)))
		{
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

			if (installerMenue)
			{
				switch(msg)
				{
					case CRCInput::RC_ok:
					case CRCInput::RC_0:
						printf("[motorcontrol] 0 key received... goto userMenue\n");
						installerMenue = false;
						paintMenu();
						paintStatus();
						break;

					case CRCInput::RC_1:
					case CRCInput::RC_right:
						printf("[motorcontrol] left/1 key received... drive/Step motor west, stepMode: %d\n", stepMode);
						motorStepWest();
						paintStatus();
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_red:
					case CRCInput::RC_2:
						printf("[motorcontrol] 2 key received... halt motor\n");
						g_Zapit->sendMotorCommand(0xE0, 0x31, 0x60, 0, 0, 0);
						moving = 0;
						paintStatus();
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_3:
					case CRCInput::RC_left:
						printf("[motorcontrol] right/3 key received... drive/Step motor east, stepMode: %d\n", stepMode);
						motorStepEast();
						paintStatus();
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_4:
						printf("[motorcontrol] 4 key received... set west (soft) limit\n");
						if(g_settings.rotor_swap) lim_cmd = 0x66;
						else lim_cmd = 0x67;
						g_Zapit->sendMotorCommand(0xE1, 0x31, lim_cmd, 0, 0, 0);
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_5:
						printf("[motorcontrol] 5 key received... disable (soft) limits\n");
						g_Zapit->sendMotorCommand(0xE0, 0x31, 0x63, 0, 0, 0);
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_6:
						printf("[motorcontrol] 6 key received... set east (soft) limit\n");
						if(g_settings.rotor_swap) lim_cmd = 0x67;
						else lim_cmd = 0x66;
						g_Zapit->sendMotorCommand(0xE1, 0x31, lim_cmd, 0, 0, 0);
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_7:
						printf("[motorcontrol] 7 key received... goto reference position\n");
						g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6B, 1, 0, 0);
						satellitePosition = 0;
						paintStatus();
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_8:
						printf("[motorcontrol] 8 key received... enable (soft) limits\n");
						g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6A, 1, 0, 0);
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_9:
						printf("[motorcontrol] 9 key received... (re)-calculate positions\n");
						g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6F, 1, 0, 0);
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_plus:
					case CRCInput::RC_up:
						printf("[motorcontrol] up key received... increase satellite position: %d\n", ++motorPosition);
						satellitePosition = 0;
						paintStatus();
						break;

					case CRCInput::RC_minus:
					case CRCInput::RC_down:
						if (motorPosition > 1) motorPosition--;
						printf("[motorcontrol] down key received... decrease satellite position: %d\n", motorPosition);
						satellitePosition = 0;
						paintStatus();
						break;

					case CRCInput::RC_blue:
						if (++stepMode > 3)
							stepMode = 0;
						if (stepMode == STEP_MODE_OFF)
							satellitePosition = 0;
						last_snr = 0;
						printf("[motorcontrol] red key received... toggle stepmode on/off: %d\n", stepMode);
						paintStatus();
						break;

					default:
						//printf("[motorcontrol] message received...\n");
						if ((msg >= CRCInput::RC_WithData) && (msg < CRCInput::RC_WithData + 0x10000000))
							delete[] (unsigned char*) data;
						break;
				}
			}
			else
			{
				switch(msg)
				{
					case CRCInput::RC_ok:
					case CRCInput::RC_0:
						printf("[motorcontrol] 0 key received... goto installerMenue\n");
						installerMenue = true;
						paintMenu();
						paintStatus();
						break;

					case CRCInput::RC_1:
					case CRCInput::RC_right:
						printf("[motorcontrol] left/1 key received... drive/Step motor west, stepMode: %d\n", stepMode);
						motorStepWest();
						paintStatus();
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_red:
					case CRCInput::RC_2:
						printf("[motorcontrol] 2 key received... halt motor\n");
						g_Zapit->sendMotorCommand(0xE0, 0x31, 0x60, 0, 0, 0);
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_3:
					case CRCInput::RC_left:
						printf("[motorcontrol] right/3 key received... drive/Step motor east, stepMode: %d\n", stepMode);
						motorStepEast();
						paintStatus();
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_green:
					case CRCInput::RC_5:
						{
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
									store = (ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, buf,CMessageBox::mbrNo,CMessageBox::mbNo|CMessageBox::mbYes) == CMessageBox::mbrYes);
								}
							}
							if(store)
								g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6A, 1, motorPosition, 0);
							//g_Zapit->tune_TP(TP);
						}
						break;

					case CRCInput::RC_6:
						if (stepSize < 0x7F) stepSize++;
						printf("[motorcontrol] 6 key received... increase Step size: %d\n", stepSize);
						paintStatus();
						break;

					case CRCInput::RC_yellow:
					case CRCInput::RC_7:
						printf("[motorcontrol] 7 key received... goto satellite number: %d\n", motorPosition);
						g_Zapit->sendMotorCommand(0xE0, 0x31, 0x6B, 1, motorPosition, 0);
						satellitePosition = 0;
						paintStatus();
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_9:
						if (stepSize > 1) stepSize--;
						printf("[motorcontrol] 9 key received... decrease Step size: %d\n", stepSize);
						paintStatus();
						break;

					case CRCInput::RC_plus:
					case CRCInput::RC_up:
						printf("[motorcontrol] up key received... increase satellite position: %d\n", ++motorPosition);
						satellitePosition = 0;
						paintStatus();
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_minus:
					case CRCInput::RC_down:
						if (motorPosition > 1) motorPosition--;
						printf("[motorcontrol] down key received... decrease satellite position: %d\n", motorPosition);
						satellitePosition = 0;
						paintStatus();
						//g_Zapit->tune_TP(TP);
						break;

					case CRCInput::RC_blue:
						if (++stepMode > 2)
							stepMode = 0;
						if (stepMode == STEP_MODE_OFF)
							satellitePosition = 0;
						printf("[motorcontrol] red key received... toggle stepmode on/off: %d\n", stepMode);
						paintStatus();
						break;

					default:
						//printf("[motorcontrol] message received...\n");
						if ((msg >= CRCInput::RC_WithData) && (msg < CRCInput::RC_WithData + 0x10000000))
							delete[] (unsigned char*) data;
						break;
				}
			}
		}
		frameBuffer->blit();

		istheend = (msg == CRCInput::RC_home);
	}

	hide();
	frontend->setTsidOnid(0);

	return menu_return::RETURN_REPAINT;
}

void CMotorControl::motorStepWest(void)
{
	int cmd;
	printf("[motorcontrol] motorStepWest\n");
	if(g_settings.rotor_swap) cmd = 0x68;
	else cmd = 0x69;
	switch(stepMode)
	{
		case STEP_MODE_ON:
			g_Zapit->sendMotorCommand(0xE0, 0x31, cmd, 1, (-1 * stepSize), 0);
			satellitePosition += stepSize;
			break;
		case STEP_MODE_TIMED:
			g_Zapit->sendMotorCommand(0xE0, 0x31, cmd, 1, 40, 0);
			usleep(stepSize * stepDelay * 1000);
			g_Zapit->sendMotorCommand(0xE0, 0x31, 0x60, 0, 0, 0); //halt motor
			satellitePosition += stepSize;
			break;
		case STEP_MODE_AUTO:
			moving = 1;
			paintStatus();
		default:
			g_Zapit->sendMotorCommand(0xE0, 0x31, cmd, 1, 40, 0);
	}
}

void CMotorControl::motorStepEast(void)
{
	int cmd;
	if(g_settings.rotor_swap) cmd = 0x69;
	else cmd = 0x68;
	printf("[motorcontrol] motorStepEast\n");
	switch(stepMode)
	{
		case STEP_MODE_ON:
			g_Zapit->sendMotorCommand(0xE0, 0x31, cmd, 1, (-1 * stepSize), 0);
			satellitePosition -= stepSize;
			break;
		case STEP_MODE_TIMED:
			g_Zapit->sendMotorCommand(0xE0, 0x31, cmd, 1, 40, 0);
			usleep(stepSize * stepDelay * 1000);
			g_Zapit->sendMotorCommand(0xE0, 0x31, 0x60, 0, 0, 0); //halt motor
			satellitePosition -= stepSize;
			break;
		case STEP_MODE_AUTO:
			moving = 1;
		default:
			g_Zapit->sendMotorCommand(0xE0, 0x31, cmd, 1, 40, 0);
	}
}

void CMotorControl::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width, height + 20);
	frameBuffer->blit();
	stopSatFind();
}

void CMotorControl::paintLine(int px, int *py, int pwidth, const char *txt)
{
	*py += mheight;
	frameBuffer->paintBoxRel(px, *py - mheight, pwidth, mheight, COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(px, *py, pwidth, txt, COL_MENUCONTENT, 0, true);
}

void CMotorControl::paintLine(int px, int py, int pwidth, const char *txt)
{
	//frameBuffer->paintBoxRel(px, py - mheight, pwidth, mheight, COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(px, py, pwidth, txt, COL_MENUCONTENT, 0, true);
}

void CMotorControl::paintSeparator(int xpos, int *pypos, int pwidth, const char * /*txt*/)
{
	//int stringwidth = 0;
	//int stringstartposX = 0;
	int th = 10;
	//*ypos += mheight;
	*pypos += th;
	frameBuffer->paintHLineRel(xpos, pwidth - 20, *pypos - (th >> 1), COL_MENUCONTENT_PLUS_3);
	frameBuffer->paintHLineRel(xpos, pwidth - 20, *pypos - (th >> 1) + 1, COL_MENUCONTENT_PLUS_1);

#if 0
	stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(txt);
	stringstartposX = 0;
	stringstartposX = (xpos + (pwidth >> 1)) - (stringwidth >> 1);
	frameBuffer->paintBoxRel(stringstartposX - 5, *pypos - mheight, stringwidth + 10, mheight, COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposX, *pypos, stringwidth, txt, COL_MENUCONTENT);
#endif
}

void CMotorControl::paintStatus()
{
	char buf[256];
	char buf2[256];

	int xpos1 = x + 10;
	int xpos2 = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_MOTORCONTROL_MOTOR_POS));
	int width2 = width - (xpos2 - xpos1) - 10;
	int width1 = width - 10;

	ypos = ypos_status;
	paintSeparator(xpos1, &ypos, width, g_Locale->getText(LOCALE_MOTORCONTROL_SETTINGS));

	paintLine(xpos1, &ypos, width1, g_Locale->getText(LOCALE_MOTORCONTROL_MOTOR_POS));
	sprintf(buf, "%d", motorPosition);
	paintLine(xpos2, ypos, width2 , buf);

	paintLine(xpos1, &ypos, width1, g_Locale->getText(LOCALE_MOTORCONTROL_MOVEMENT));
	switch(stepMode)
	{
		case STEP_MODE_ON:
			strcpy(buf, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_MODE));
			break;
		case STEP_MODE_OFF:
			strcpy(buf, g_Locale->getText(LOCALE_MOTORCONTROL_DRIVE_MODE));
			break;
		case STEP_MODE_AUTO:
			strcpy(buf, g_Locale->getText(LOCALE_MOTORCONTROL_DRIVE_MODE_AUTO));
			break;
		case STEP_MODE_TIMED:
			strcpy(buf, g_Locale->getText(LOCALE_MOTORCONTROL_TIMED_MODE));
			break;
	}
	paintLine(xpos2, ypos, width2, buf);

	paintLine(xpos1, &ypos, width1, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_SIZE));
	switch(stepMode)
	{
		case STEP_MODE_ON:
			sprintf(buf, "%d", stepSize);
			break;
		case STEP_MODE_AUTO:
			if(moving)
				strcpy(buf, g_Locale->getText(LOCALE_MOTORCONTROL_STOP_MOVING));
			else
				strcpy(buf, g_Locale->getText(LOCALE_MOTORCONTROL_STOP_STOPPED));
			break;
		case STEP_MODE_OFF:
			strcpy(buf, g_Locale->getText(LOCALE_MOTORCONTROL_NO_MODE));
			break;
		case STEP_MODE_TIMED:
			sprintf(buf, "%d ", stepSize * stepDelay);
			strcat(buf, g_Locale->getText(LOCALE_MOTORCONTROL_MSEC));
			break;
	}
	paintLine(xpos2, ypos, width2, buf);

	paintSeparator(xpos1, &ypos, width, g_Locale->getText(LOCALE_MOTORCONTROL_STATUS));
	strcpy(buf, g_Locale->getText(LOCALE_MOTORCONTROL_SAT_POS));
	sprintf(buf2, "%d", satellitePosition);
	strcat(buf, buf2);
	paintLine(xpos1, &ypos, width1, buf);
	paintSeparator(xpos1, &ypos, width, g_Locale->getText(LOCALE_MOTORCONTROL_SETTINGS));
}

void CMotorControl::paintHead()
{
	CComponentsHeader header(x, y, width, hheight, LOCALE_MOTORCONTROL_HEAD, NULL /*no header icon*/);
	header.paint(CC_SAVE_SCREEN_NO);
}

void CMotorControl::paintMenu()
{
	frameBuffer->paintBoxRel(x, y + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	ypos = y + hheight + (mheight >> 1) - 10;

	int xpos1 = x + 10;
	int xpos2 = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("(7/yellow)");
	int width2 = width - (xpos2 - xpos1) - 10;
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
		paintLine(xpos1, &ypos, width1, "(5)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_DISABLE_LIMIT));
		paintLine(xpos1, &ypos, width1, "(6)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_EAST_LIMIT));
		paintLine(xpos1, &ypos, width1, "(7)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_REF_POSITION));
		paintLine(xpos1, &ypos, width1, "(8)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_ENABLE_LIMIT));
		paintLine(xpos1, &ypos, width1, "(9)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_CALC_POSITIONS));
		paintLine(xpos1, &ypos, width1, "(+/up)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_POS_INCREASE));
		paintLine(xpos1, &ypos, width1, "(-/down)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_POS_DECREASE));
		paintLine(xpos1, &ypos, width1, "(blue)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_DRIVE));
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
		paintLine(xpos1, &ypos, width1, "(+/up)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_POS_INCREASE));
		paintLine(xpos1, &ypos, width1, "(-/down)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_POS_DECREASE));
		paintLine(xpos1, &ypos, width1, "(blue)");
		paintLine(xpos2, ypos, width2, g_Locale->getText(LOCALE_MOTORCONTROL_STEP_DRIVE));
	}

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

	if (satfindpid != -1) {
		printf("[motorcontrol] killing satfind...\n");
		kill(satfindpid, SIGKILL);
		waitpid(satfindpid, 0, 0);
		satfindpid = -1;
	}
}

#define BARWT 10
#define BAR_BL 2
#define BARW (BARWT - BAR_BL)
#define BARWW (BARWT - BARW)

void CMotorControl::showSNR()
{
	char percent[10];
	//char ber[20];
	int barwidth = 100;
	uint16_t ssig, ssnr;
	int sig, snr;
	int posx_sig, posx_snr, posy;

	int sw;

	ssig = frontend->getSignalStrength();
	ssnr = frontend->getSignalNoiseRatio();

	snr = (ssnr & 0xFFFF) * 100 / 65535;
	sig = (ssig & 0xFFFF) * 100 / 65535;
	if(sig < 5)
		return;
	g_sig = ssig & 0xFFFF;
	g_snr = snr;

	posy = y + height - mheight - 5;
	//TODO: move sig/snr display into its own class, similar or same code also to find in scan.cpp
	if (lastsig != sig) {
		lastsig = sig;
		posx_sig = x + 10;
		sprintf(percent, "%d%% SIG", sig);
		sw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth ("100% SIG");

		sigscale->setProgress(posx_sig-1, posy, BAR_WIDTH, BAR_HEIGHT, sig, 100);
		sigscale->paint();

		posx_sig += barwidth + 3;
		frameBuffer->paintBoxRel(posx_sig, posy - 2, sw+4, mheight, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx_sig+2, posy + mheight, sw, percent, COL_MENUCONTENT);
	}

	if (lastsnr != snr) {
		lastsnr = snr;
		posx_snr = x + 10 + 210;
		sprintf(percent, "%d%% SNR", snr);
		sw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth ("100% SNR");
		
		snrscale->setProgress(posx_snr-1, posy, BAR_WIDTH, BAR_HEIGHT, snr, 100);
 		snrscale->paint();

		posx_snr += barwidth + 3;
		frameBuffer->paintBoxRel(posx_snr, posy - 2, sw+4, mheight, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx_snr+2, posy + mheight, sw, percent, COL_MENUCONTENT);
	}
}
