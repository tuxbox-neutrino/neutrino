/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Shell window class, visualize of system events on gui screen.

	Implementation:
	Copyright (C) 2013 martii
	gitorious.org/neutrino-mp/martiis-neutrino-mp

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
#include <driver/framebuffer.h>
#include <gui/widget/textbox.h>
#include <stdio.h>
#include <poll.h>
#include <fcntl.h>

CShellWindow::CShellWindow(const std::string &command, const int _mode, int *res) {
	textBox = NULL;
	std::string cmd;
	mode = _mode;
	if (!(mode & VERBOSE)){
		cmd = "PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin ; export PATH ; " + command + " 2>/dev/null >&2";
		int r = system(cmd.c_str());
		if (res) {
			if (r == -1)
				*res = r;
			else
				*res = WEXITSTATUS(r);
		}
		return;
	}

	cmd = command + " 2>&1";
	FILE *f = popen(cmd.c_str(), "r");
	if (!f) {
		if (res)
			*res = -1;
		return;
	}
	Font *font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO];
	frameBuffer = CFrameBuffer::getInstance();
	unsigned int lines_max = frameBuffer->getScreenHeight() / font->getHeight();
	list<std::string> lines;
	CBox textBoxPosition(frameBuffer->getScreenX(), frameBuffer->getScreenY(), frameBuffer->getScreenWidth(), frameBuffer->getScreenHeight());
	textBox = new CTextBox(cmd.c_str(), font, CTextBox::BOTTOM, &textBoxPosition);
	struct pollfd fds;
	fds.fd = fileno(f);
	fds.events = POLLIN | POLLHUP | POLLERR;
	fcntl(fds.fd, F_SETFL, fcntl(fds.fd, F_GETFL, 0) | O_NONBLOCK);

	struct timeval tv;
	gettimeofday(&tv,NULL);
	uint64_t lastPaint = (uint64_t) tv.tv_usec + (uint64_t)((uint64_t) tv.tv_sec * (uint64_t) 1000000);
	bool ok = true, nlseen = false, dirty = false, pushed = false;
	char output[1024];
	int off = 0;
	std::string txt = "";

	do {
		uint64_t now;
		fds.revents = 0;
		int r = poll(&fds, 1, 300);

		if (r > 0) {
			if (!feof(f)) {
				gettimeofday(&tv,NULL);
				now = (uint64_t) tv.tv_usec + (uint64_t)((uint64_t) tv.tv_sec * (uint64_t) 1000000);

				unsigned int lines_read = 0;
				while (fgets(output + off, sizeof(output) - off, f)) {
					char *outputp = output + off;
					dirty = true;

					for (int i = off; output[i] && !nlseen; i++)
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
					if (nlseen) {
						pushed = false;
						nlseen = false;
						off = 0;
					} else {
						off = strlen(output);
						if (pushed)
							lines.pop_back();
					}
					lines.push_back(std::string((output)));
					pushed = true;
					if (lines.size() > lines_max)
						lines.pop_front();
					txt = "";
					bool first = true;
					for (std::list<std::string>::const_iterator it = lines.begin(), end = lines.end(); it != end; ++it) {
						if (first)
							first = false;
						else
							txt += '\n';
						txt += *it;
					}
					if (((lines_read == lines_max) && (lastPaint + 100000 < now)) || (lastPaint + 250000 < now)) {
						textBox->setText(&txt);
						textBox->paint();
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
		if (r < 1 || dirty || lastPaint + 250000 < now) {
			textBox->setText(&txt);
			textBox->paint();
			lastPaint = now;
			dirty = false;
		}
	} while(ok);

	int r = pclose(f);

	if (res) {
		if (r == -1)
			*res = r;
		else
			*res = WEXITSTATUS(r);
	}
}

CShellWindow::~CShellWindow()
{
	if (textBox && (mode & ACKNOWLEDGE)) {
		int iw, ih;
		frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_OKAY, &iw, &ih);
		Font *font = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];
		int b_width = font->getRenderWidth(g_Locale->getText(LOCALE_MESSAGEBOX_OK)) + 36 + ih + (RADIUS_LARGE / 2);
		int fh = font->getHeight();
		int b_height = std::max(fh, ih) + 8 + (RADIUS_LARGE / 2);
		int xpos = frameBuffer->getScreenWidth() - b_width;
		int ypos = frameBuffer->getScreenHeight() - b_height;
		frameBuffer->paintBoxRel(xpos, ypos, b_width, b_height, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE);
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_OKAY, xpos + ((b_height - ih) / 2), ypos + ((b_height - ih) / 2), ih);
		font->RenderString(xpos + iw + 17, ypos + fh + ((b_height - fh) / 2), b_width - (iw + 21), g_Locale->getText(LOCALE_MESSAGEBOX_OK), COL_MENUCONTENT_TEXT);
		frameBuffer->blit();

		neutrino_msg_t msg;
		neutrino_msg_data_t data;
		do
			g_RCInput->getMsg(&msg, &data, 100);
		while (msg != CRCInput::RC_ok && msg != CRCInput::RC_home);

		frameBuffer->Clear();
		frameBuffer->blit();
	}
	if (textBox) {
		textBox->hide();
		delete textBox;
	}
}
