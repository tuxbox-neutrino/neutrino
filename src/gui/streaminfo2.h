/*
	Neutrino-GUI  -   DBoxII-Project


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


#ifndef __streaminfo2__
#define __streaminfo2__

#include <gui/widget/menue.h>

#include <gui/components/cc.h>
#include <gui/movieplayer.h>
#include <zapit/femanager.h>
#include <vector>
#include <map>

struct AVFormatContext;

class CFrameBuffer;
class CStreamInfo2 : public CMenuTarget
{
	private:

		CFrameBuffer	*frameBuffer;
		CFrontend	*frontend;
		CComponentsPIP	*pip;
		CMoviePlayerGui	*mp;
		COSDFader	fader;
		int x;
		int y;
		int width;
		int height;
		int hheight, iheight, sheight; 	// head/info/small font height

		int max_height;	// Frambuffer 0.. max
		int max_width;

		int yypos;
		int paint_mode;

		int font_head;
		int font_info;
		int font_small;

		int sigBox_x;
		int sigBox_y;
		int sigBox_w;
		int sigBox_h;
		int sigBox_pos;
		int sig_text_y;
		int sig_text_w;
		int sig_text_ber_x;
		int sig_text_sig_x;
		int sig_text_snr_x;
		int sig_text_rate_x;
		int average_bitrate_pos;

		int techinfo_xpos, techinfo_ypos;
		int box_width;

		int spaceoffset;
		unsigned int scaling;
		unsigned int pmt_version;
		int box_h, box_h2;
		struct feSignal
		{
			unsigned long	ber, old_ber, max_ber, min_ber;
			unsigned long	sig, old_sig, max_sig, min_sig;
			unsigned long	snr, old_snr, max_snr, min_snr;
		} signal;

		struct bitrate
		{
			unsigned int short_average, max_short_average, min_short_average;
		} rate;

		std::vector<std::map<std::string, std::string> > streamdata;

		int doSignalStrengthLoop();

		struct timeval tv, last_tv, first_tv;
		uint64_t bit_s;
		uint64_t abit_s;
		uint64_t b_total;
		unsigned char *dmxbuf;
		unsigned char *probebuf;
		unsigned int probebuf_off;
		unsigned int probebuf_size;
		unsigned int probebuf_length;
		OpenThreads::Mutex probe_mutex;
		pthread_t probe_thread;
		bool probed;

		bool update_rate();
		bool ts_setup();
		int ts_close();
		static void *probeStreams(void *arg);
		void probeStreams();
		void analyzeStreams(AVFormatContext *avfc);
		void analyzeStream(AVFormatContext *avfc, unsigned int i);

		void paint(int mode);
		void paint_techinfo(int x, int y);
		void paintCASystem(int xpos, int ypos);
		void paint_signal_fe_box(int x, int y, int w, int h);
		void paint_signal_fe(struct bitrate rate, struct feSignal s);
		int y_signal_fe(unsigned long value, unsigned long max_range, int max_y);
		void SignalRenderHead(std::string head, int x, int y, fb_pixel_t color);
		void SignalRenderStr(unsigned int value, int x, int y);
		CSignalBox *signalbox;

		void showSNR();
	public:
		bool abort_probing;

		CStreamInfo2();
		~CStreamInfo2();

		void hide();
		int exec(CMenuTarget *parent, const std::string &actionKey);

		int readPacket(uint8_t *buf, int buf_size);
};
#endif

