/*
 * OSC sequence handlers for the YAFT framebuffer terminal integration in
 * Neutrino.
 * Copyright (C) 2018 Stefan Seyfried
 *
 * Neutrino integration license: GPL-2.0-only.
 *
 * This file contains code derived from upstream yaft/ctrlseq/osc.h
 * (https://github.com/uobikiemukot/yaft), originally:
 * Copyright (c) 2012 haru <uobikiemukot at gmail dot com>
 *
 * Upstream yaft is MIT-licensed; see LICENSE in this directory for the
 * original permission notice and warranty disclaimer. This integration is
 * distributed with Neutrino under the GPL terms above, without warranty.
 */
#include "yaft_priv.h"
#include <cstring>

/* function for osc sequence */
int32_t YaFT_p::parse_color1(std::string &seq)
{
	/*
	format
		rgb:r/g/b
		rgb:rr/gg/bb
		rgb:rrr/ggg/bbb
		rgb:rrrr/gggg/bbbb
	*/
	int i, length, value;
	int32_t color;
	uint32_t rgb[3];
	struct parm_t parm;

	reset_parm(&parm);
	parse_arg(seq, &parm, '/', isalnum);

	for (i = 0; i < parm.argc; i++)
		logging(DEBUG, "parm.argv[%d]: %s\n", i, parm.argv[i].c_str());

	if (parm.argc != 3)
		return -1;

	length = parm.argv[0].length();

	for (i = 0; i < 3; i++) {
		value = hex2num(parm.argv[i]);
		logging(DEBUG, "value:%d\n", value);

		if (length == 1)      /* r/g/b/ */
			rgb[i] = 0xFF & (value * 0xFF / 0x0F);
		else if (length == 2) /* rr/gg/bb */
			rgb[i] = 0xFF & value;
		else if (length == 3) /* rrr/ggg/bbb */
			rgb[i] = 0xFF & (value * 0xFF / 0xFFF);
		else if (length == 4) /* rrrr/gggg/bbbb */
			rgb[i] = 0xFF & (value * 0xFF / 0xFFFF);
		else
			return -1;
	}

	color = (rgb[0] << 16) + (rgb[1] << 8) + rgb[2];
	logging(DEBUG, "color:0x%.6X\n", color);

	return color;
}

int32_t YaFT_p::parse_color2(std::string &seq)
{
	/*
	format
		#rgb
		#rrggbb
		#rrrgggbbb
		#rrrrggggbbbb
	*/
	int i, length;
	uint32_t rgb[3];
	int32_t color;
	char buf[BUFSIZE];

	length = seq.length();
	memset(buf, '\0', BUFSIZE);

	if (length == 3) {       /* rgb */
		for (i = 0; i < 3; i++) {
			rgb[i] = 0xFF & hex2num(seq.substr(i, 1)) * 0xFF / 0x0F;
		}
	} else if (length == 6) {  /* rrggbb */
		for (i = 0; i < 3; i++) { /* rrggbb */
			rgb[i] = 0xFF & hex2num(seq.substr(i * 2, 2));
		}
	} else if (length == 9) {  /* rrrgggbbb */
		for (i = 0; i < 3; i++) {
			rgb[i] = 0xFF & hex2num(seq.substr(i * 3, 3)) * 0xFF / 0xFFF;
		}
	} else if (length == 12) { /* rrrrggggbbbb */
		for (i = 0; i < 3; i++) {
			rgb[i] = 0xFF & hex2num(seq.substr(i * 4, 4)) * 0xFF / 0xFFFF;
		}
	} else {
		return -1;
	}

	color = (rgb[0] << 16) + (rgb[1] << 8) + rgb[2];
	logging(DEBUG, "color:0x%.6X\n", color);

	return color;
}

void YaFT_p::set_palette(struct parm_t *pt)
{
	/*
	OSC Ps ; Pt ST
	ref: http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
	ref: http://ttssh2.sourceforge.jp/manual/ja/about/ctrlseq.html#OSC

	only recognize change color palette:
		Ps: 4
		Pt: c ; spec
			c: color index (from 0 to 255)
			spec:
				rgb:r/g/b
				rgb:rr/gg/bb
				rgb:rrr/ggg/bbb
				rgb:rrrr/gggg/bbbb
				#rgb
				#rrggbb
				#rrrgggbbb
				#rrrrggggbbbb
					this rgb format is "RGB Device String Specification"
					see http://xjman.dsl.gr.jp/X11R6/X11/CH06.html
		Pt: c ; ?
			response rgb color
				OSC 4 ; c ; rgb:rr/gg/bb ST

	TODO: this function only works in 32bpp mode
	*/
	int i, argc = pt->argc, index;
	int32_t color;
	uint8_t rgb[3];
	std::string *argv = pt->argv;
	char buf[BUFSIZE];

	if (argc != 3)
		return;

	index = dec2num(argv[1]);
	if (index < 0 || index >= COLORS)
		return;

	if (argv[2].compare(0, 4, "rgb:") == 0) {
		std::string tmp = argv[2].substr(4); /* skip "rgb:" */
		if ((color = parse_color1(tmp)) != -1) {
			virtual_palette[index] = (uint32_t) color;
			palette_modified = true;
		}
	} else if (argv[2][0] == '#') {
		std::string tmp = argv[2].substr(1); /* skip "#" */
		if ((color = parse_color2(tmp)) != -1) {
			virtual_palette[index] = (uint32_t) color;
			palette_modified = true;
		}
	} else if (argv[2][0] == '?') {
		for (i = 0; i < 3; i++)
			rgb[i] = 0xFF & (virtual_palette[index] >> (8 * (2 - i)));

		snprintf(buf, BUFSIZE, "\033]4;%d;rgb:%.2X/%.2X/%.2X\033\\",
			index, rgb[0], rgb[1], rgb[2]);
		ssize_t ignored __attribute__((unused)) = write(fd, buf, strlen(buf));
	}
}

void YaFT_p::reset_palette(struct parm_t *pt)
{
	/*
	reset color c
		OSC 104 ; c ST
			c: index of color
			ST: BEL or ESC \
	reset all color
		OSC 104 ST
			ST: BEL or ESC \

	terminfo: oc=\E]104\E\\
	*/
	int i, argc = pt->argc, c;
	std::string *argv = pt->argv;

	if (argc < 2) { /* reset all color palette */
		for (i = 0; i < COLORS; i++)
			virtual_palette[i] = color_list[i];
		palette_modified = true;
	} else if (argc == 2) { /* reset color_palette[c] */
		c = dec2num(argv[1]);
		if (0 <= c && c < COLORS) {
			virtual_palette[c] = color_list[c];
			palette_modified = true;
		}
	}
}
