/*
 * Terminal window class
 *
 * (C) 2017,2017 Stefan Seyfried
 * SPDX-License-Identifier: GPL-2.0
 *
 * drop-in-replacement for CShellWindow()
 * loosely based on shellwindow.cpp, heavily stripped down.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "termwindow.h"

#include <global.h>
#include <neutrino.h>
#include <driver/framebuffer.h>
#include <stdio.h>
#include <errno.h>
#include <system/helpers.h>
#include <system/debug.h>
#include <gui/widget/yaft/yaft_class.h>
#include <gui/widget/msgbox.h>

CTermWindow::CTermWindow(const std::string &Command, const int Mode, int *Res, bool auto_exec)
{
	fprintf(stderr, "%s:%d mode %d\n", __func__, __LINE__, Mode);
	frameBuffer = CFrameBuffer::getInstance();

	setCommand(Command, Mode, Res, auto_exec);
}

void CTermWindow::setCommand(const std::string &Command, const int Mode, int* Res, bool auto_exec)
{
	command = Command;
	mode = Mode;
	fprintf(stderr, "%s:%d mode %d\n", __func__, __LINE__, mode);
	res = Res;
	if (auto_exec)
		exec();
}

void CTermWindow::exec()
{
	fprintf(stderr, "CTermWindow::exec: mode %d command: %s\n", mode, command.c_str());
	std::string cmd;
	if (mode == 0){ /* not used */
		cmd = "PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin ; export PATH ; " + command + " 2>/dev/null >&2";
		int r = my_system(cmd.c_str());
		if (res) {
			if (r == -1)
				*res = r;
			else
				*res = WEXITSTATUS(r);
			dprintf(DEBUG_NORMAL,  "[CTermWindow] [%s - %d]  Error! system returns: %d command: %s\n", __func__, __LINE__, *res, cmd.c_str());
		}
	}
	else {
		const char * const argv[] = {"/bin/sh\0", "-c\0", command.c_str(), NULL };
		int ret;
fprintf(stderr, "%s:%d %s\n", __func__, __LINE__, argv[0]);
		YaFT *y = new YaFT(argv, res, !!(mode & VERBOSE), OnShellOutputLoop);
		ret = y->run();
		if (res)
			*res = ret;
		delete y;
		y = NULL;
fprintf(stderr, "%s:%d\n", __func__, __LINE__);
		showResult();
	}
}

void CTermWindow::showResult()
{
	if (mode == 0) /* not used */
		return;

	bool show_button = false;
	bool exit = false;

	if (mode & ACKNOWLEDGE)
		show_button = true;
	else if (mode & ACKNOWLEDGE_EVENT) {
		if (res && *res != 0){
			OnResultError(res);
			if (OnResultError.empty())
				DisplayErrorMessage("Error while execution of task. Please see window for details!");
			show_button = true;
		}else{
			OnResultOk(res);
			exit = true; //TODO: evaluate plausible statement
		}
	}

	if (mode & VERBOSE) {
		if (show_button) {
			int b_width = 150;
			int b_height = 35;
			/*
			   we need the dimensions of yaft window to place the button correctly
			*/
			int xpos = frameBuffer->getScreenWidth() - b_width;
			int ypos = frameBuffer->getScreenHeight() - b_height;
			CComponentsButton btn(xpos, ypos, b_width, b_height, LOCALE_MESSAGEBOX_BACK, NEUTRINO_ICON_BUTTON_OKAY, NULL, true, true);
			btn.setColorBody(COL_MENUCONTENT_PLUS_0);
			btn.paint(false);
		}

		neutrino_msg_t msg;
		neutrino_msg_data_t data;
		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if (!exit) {
			do {
				g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
				if ( msg >  CRCInput::RC_MaxRC && msg != CRCInput::RC_ok && msg != CRCInput::RC_home && msg != CRCInput::RC_timeout){
					CNeutrinoApp::getInstance()->handleMsg( msg, data );
				}

			} while (msg != CRCInput::RC_ok && msg != CRCInput::RC_home && msg != CRCInput::RC_timeout);
		}
		frameBuffer->Clear();
	}
}

#if 0
// only for debug
void CTermWindow::handleShellOutput(std::string* cur_line, int* Res, bool* /*ok*/)
{
	int _res = *Res;
	std::string line = *cur_line;
	fprintf(stderr, "%s\n", line.c_str());
	*Res = _res;
}
#endif

CTermWindow::~CTermWindow()
{
}
