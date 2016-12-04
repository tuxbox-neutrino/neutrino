/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Shell window class, visualize of system events on gui screen.

	Implementation:
	Copyright (C) 2013 martii
	gitorious.org/neutrino-mp/martiis-neutrino-mp
	Copyright (C) 2015 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "shellwindow.h"

#include <global.h>
#include <neutrino.h>
#include <sys/wait.h>
#include <driver/framebuffer.h>
#include <gui/widget/textbox.h>
#include <stdio.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <system/helpers.h>
#include <gui/widget/msgbox.h>
#include <errno.h>
#include <system/debug.h>

CShellWindow::CShellWindow(const std::string &Command, const int Mode, int *Res, bool auto_exec)
{
	textBox 	= NULL;
	frameBuffer 	= CFrameBuffer::getInstance();

	setCommand(Command, Mode, Res, auto_exec);
}

void CShellWindow::setCommand(const std::string &Command, const int Mode, int* Res, bool auto_exec)
{
	command = Command;
	mode = Mode;
	res = Res;
	if (auto_exec)
		exec();
}

void CShellWindow::exec()
{
	std::string cmd;
	if (mode == 0){
		cmd = "PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin ; export PATH ; " + command + " 2>/dev/null >&2";
		int r = my_system(cmd.c_str());
		if (res) {
			if (r == -1)
				*res = r;
			else
				*res = WEXITSTATUS(r);
			dprintf(DEBUG_NORMAL,  "[CShellWindow] [%s - %d]  Error! system returns: %d command: %s\n", __func__, __LINE__, *res, cmd.c_str());
		}
	}
	else {
		pid_t pid = 0;
		cmd = command + " 2>&1";
		FILE *f = my_popen(pid, cmd.c_str(), "r");
		if (!f) {
			if (res)
				*res = -1;
			dprintf(DEBUG_NORMAL, "[CShellWindow] [%s:%d]  Error! my_popen errno: %d command: %s\n", __func__, __LINE__, errno, cmd.c_str());
			return;
		}

		Font *font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO];
		int h_shell = frameBuffer->getScreenHeight();
		int w_shell = frameBuffer->getScreenWidth();
		unsigned int lines_max = h_shell / font->getHeight();
		list<std::string> lines;
		CBox textBoxPosition(frameBuffer->getScreenX(), frameBuffer->getScreenY(), w_shell, h_shell);
		if (textBox == NULL){
			textBox = new CTextBox(cmd.c_str(), font, CTextBox::BOTTOM, &textBoxPosition);
			textBox->enableSaveScreen(false);
		}
		struct pollfd fds;
		fds.fd = fileno(f);
		fds.events = POLLIN | POLLHUP | POLLERR;
		fcntl(fds.fd, F_SETFL, fcntl(fds.fd, F_GETFL, 0) | O_NONBLOCK);

		struct timeval tv;
		gettimeofday(&tv,NULL);
		uint64_t lastPaint = (uint64_t) tv.tv_usec + (uint64_t)((uint64_t) tv.tv_sec * (uint64_t) 1000000);
		bool ok = true, nlseen = false, dirty = false, incomplete = false;
		char output[1024];
		std::string txt = "";
		std::string line = "";

		do {
			uint64_t now;
			fds.revents = 0;
			int r = poll(&fds, 1, 300);
			if (r > 0) {
				if (!feof(f)) {
					gettimeofday(&tv,NULL);
					now = (uint64_t) tv.tv_usec + (uint64_t)((uint64_t) tv.tv_sec * (uint64_t) 1000000);

					unsigned int lines_read = 0;
					while (fgets(output, sizeof(output), f)) {
						char *outputp = output;
						dirty = true;

						for (int i = 0; output[i] && !nlseen; i++)
							switch (output[i]) {
								case '\b':
									if (outputp > output)
										outputp--;
									*outputp = 0;
									break;
								case '\r':
									outputp = output;
									break;
								case '\n':
									lines_read++;
									nlseen = true;
									*outputp = 0;
									break;
								default:
									*outputp++ = output[i];
									break;
							}

						if (outputp < output + sizeof(output))
							*outputp = 0;
						line += std::string(output);
						if (incomplete)
							lines.pop_back();
						if (nlseen) {
							lines.push_back(line);
							line.clear();
							nlseen = false;
							incomplete = false;
						} else {
							lines.push_back(line);
							incomplete = true;
						}

						//callback for line handler
						std::string s_output = std::string((output));
						OnShellOutputLoop(&s_output, res, &ok);
						if (res)
							dprintf(DEBUG_NORMAL, "[CShellWindow] [%s:%d] res=%d ok=%d\n", __func__, __LINE__, *res, ok);
						else
							dprintf(DEBUG_NORMAL, "[CShellWindow] [%s:%d] res=NULL ok=%d\n", __func__, __LINE__, ok);

						if (lines.size() > lines_max)
							lines.pop_front();
						txt = "";
						bool first = true;
						for (std::list<std::string>::const_iterator it = lines.begin(), end = lines.end(); it != end; ++it) {
							if (!first)
								txt += '\n';
							first = false;
							txt += *it;
						}
						if (((lines_read == lines_max) && (lastPaint + 100000 < now)) || (lastPaint + 250000 < now)) {
							textBox->setText(&txt, textBox->getWindowsPos().iWidth, false);
							if (!textBox->isPainted())
								if (mode & VERBOSE) textBox->paint();
							lines_read = 0;
							lastPaint = now;
							dirty = false;
						}
					}
				} else
					ok = false;
			} else if (r < 0)
				ok = false;

			gettimeofday(&tv,NULL);
			now = (uint64_t) tv.tv_usec + (uint64_t)((uint64_t) tv.tv_sec * (uint64_t) 1000000);
			if (!ok || (r < 1 && dirty && lastPaint + 250000 < now)) {
				textBox->setText(&txt, textBox->getWindowsPos().iWidth, false);
				if (!textBox->isPainted())
					if (mode & VERBOSE) textBox->paint();
				lastPaint = now;
				dirty = false;
			}
		} while(ok);

		if (mode & VERBOSE) {
			txt += "\n...ready";
			textBox->setText(&txt, textBox->getWindowsPos().iWidth, false);
		}

		fclose(f);
		int s;
		errno = 0;
		int r = waitpid(pid, &s, 0);

		if (res){
			//if res value was generated inside signal, then use foreign res from signal instead own res value
			if (OnShellOutputLoop.empty()){
				if (r == -1)
					*res = errno;
				else
					*res = WEXITSTATUS(s);
			}
		}

		showResult();
	}
}

void CShellWindow::showResult()
{
	if (textBox){
		bool show_button = false;
		bool exit = false;

		if (mode & ACKNOWLEDGE){
			show_button = true;
		}
		else if (mode & ACKNOWLEDGE_EVENT){
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

		if ((mode & VERBOSE)){
			if (show_button){
				int b_width = 150;
				int b_height = 35;
				int xpos = frameBuffer->getScreenWidth() - b_width;
				int ypos = frameBuffer->getScreenHeight() - b_height;
				CComponentsButton btn(xpos, ypos, b_width, b_height, LOCALE_MESSAGEBOX_BACK, NEUTRINO_ICON_BUTTON_OKAY, NULL, true, true);
				btn.setColorBody(COL_MENUCONTENT_PLUS_0);
				btn.paint(false);
			}

			neutrino_msg_t msg;
			neutrino_msg_data_t data;
			uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

			if (!exit)
			{
				do
					g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
				while (msg != CRCInput::RC_ok && msg != CRCInput::RC_home && msg != CRCInput::RC_timeout);
			}
			textBox->hide();
		}
	}
}

CShellWindow::~CShellWindow()
{
	if (textBox)
		delete textBox;
}
