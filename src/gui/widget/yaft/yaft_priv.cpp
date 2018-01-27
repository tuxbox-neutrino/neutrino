/*
 * yaft framebuffer terminal as C++ class for embedding in neutrino-MP
 * (C) 2018 Stefan Seyfried
 * License: GPL-2.0
 *
 * this file contains the private YaFT_p class with "backend" functionality
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * derived from yaft https://github.com/uobikiemukot/yaft
 * original code
 * Copyright (c) 2012 haru <uobikiemukot at gmail dot com>
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 */

#include "yaft_priv.h"
#include "glyph.h"
#include <driver/framebuffer.h>
#include <driver/abstime.h>

/* parse_arg functions */
void YaFT_p::reset_parm(parm_t *pt)
{
	pt->argc = 0;
	for (int i = 0; i < MAX_ARGS; i++)
		pt->argv[i].clear();
}

void add_parm(struct parm_t *pt, std::string cp)
{
	if (pt->argc >= MAX_ARGS)
		return;

	logging(DEBUG, "argv[%d]: %s\n", pt->argc, cp.c_str());

	pt->argv[pt->argc] = cp;
	pt->argc++;
}

void YaFT_p::parse_arg(std::string &buf, struct parm_t *pt, int delim, int (is_valid)(int c))
{
	/*
		v..........v d           v.....v d v.....v ... d
		(valid char) (delimiter)
		argv[0]                  argv[1]   argv[2] ...   argv[argc - 1]
	*/
	size_t length;
	size_t vp = std::string::npos;
	int c;

	length = buf.length();
	logging(DEBUG, "parse_arg() length:%u\n", (unsigned) length);

	for (size_t i = 0; i < length; i++) {
		c = buf[i];

		if (vp == std::string::npos && is_valid(c))
			vp = i;

		if (c == delim) {
			add_parm(pt, buf.substr(vp, i - vp));
			vp = std::string::npos;
		}

		if (i == (length - 1) && (vp != std::string::npos || c == 0))
			add_parm(pt, buf.substr(vp));
	}

	logging(DEBUG, "argc:%d\n", pt->argc);
}

/* constructor, Paint == false means "quiet mode, just execute
 * a command but don't display anything */
YaFT_p::YaFT_p(bool Paint)
{
	lines_available = 0;
	txt.push("");
	nlseen = false;
	paint = Paint;
	last_paint = 0;
}

bool YaFT_p::init()
{
	fb.cfb = CFrameBuffer::getInstance();
	fb.buf = (uint32_t *)fb.cfb->getBackBufferPointer();
	fb.width  = fb.cfb->getScreenWidth();
	fb.height = fb.cfb->getScreenHeight();
	fb.xstart = fb.cfb->getScreenX();
	fb.ystart = fb.cfb->getScreenY();
	fb.line_length = width * sizeof(fb_pixel_t);
	fb.screen_size = fb.line_length * fb.height;
	fb.dy_min = fb.height;
	fb.dy_max = -1;
	screeninfo = fb.cfb->getScreenInfo();

	return term_init(fb.width, fb.height);
}

void YaFT_p::erase_cell(int y, int x)
{
	struct cell_t *cellp;

	cellp             = &cells[y][x];
	cellp->glyphp     = glyph[DEFAULT_CHAR];
	cellp->color_pair = color_pair; /* bce */
	cellp->attribute  = ATTR_RESET;
	line_dirty[y] = true;
}

void YaFT_p::copy_cell(int dst_y, int dst_x, int src_y, int src_x)
{
	struct cell_t *dst, *src;

	dst = &cells[dst_y][dst_x];
	src = &cells[src_y][src_x];
	*dst = *src;
	line_dirty[dst_y] = true;
}

int YaFT_p::set_cell(int y, int x, const struct glyph_t *glyphp)
{
	struct cell_t cell; //, *cellp;
	uint8_t color_tmp;

	cell.glyphp = glyphp;

	cell.color_pair.fg = (attribute & attr_mask[ATTR_BOLD] && color_pair.fg <= 7) ?
		color_pair.fg + BRIGHT_INC : color_pair.fg;
	cell.color_pair.bg = (attribute & attr_mask[ATTR_BLINK] && color_pair.bg <= 7) ?
		color_pair.bg + BRIGHT_INC : color_pair.bg;

	if (attribute & attr_mask[ATTR_REVERSE]) {
		color_tmp          = cell.color_pair.fg;
		cell.color_pair.fg = cell.color_pair.bg;
		cell.color_pair.bg = color_tmp;
	}

	cell.attribute  = attribute;

	cells[y][x] = cell;
	line_dirty[y] = true;
	return 1;
}

void YaFT_p::swap_lines(int i, int j)
{
	std::swap(cells[i], cells[j]);
}

void YaFT_p::scroll(int from, int to, int offset)
{
	int abs_offset, scroll_lines;

	if (offset == 0 || from >= to)
		return;

	logging(DEBUG, "scroll from:%d to:%d offset:%d\n", from, to, offset);

	for (int y = from; y <= to; y++)
		line_dirty[y] = true;

	abs_offset = abs(offset);
	scroll_lines = (to - from + 1) - abs_offset;

	if (offset > 0) { /* scroll down */
		for (int y = from; y < from + scroll_lines; y++)
			swap_lines(y, y + offset);
		for (int y = (to - offset + 1); y <= to; y++)
			for (int x = 0; x < cols; x++)
				erase_cell(y, x);
	}
	else {            /* scroll up */
		for (int y = to; y >= from + abs_offset; y--)
			swap_lines(y, y - abs_offset);
		for (int y = from; y < from + abs_offset; y++)
			for (int x = 0; x < cols; x++)
				erase_cell(y, x);
	}
}

/* relative movement: cause scrolling */
void YaFT_p::move_cursor(int y_offset, int x_offset)
{
	int x, y, top, bottom;

	x = cursor.x + x_offset;
	y = cursor.y + y_offset;

	top    = scrollm.top;
	bottom = scrollm.bottom;

	if (x < 0) {
		x = 0;
	} else if (x >= cols) {
		if (mode & MODE_AMRIGHT)
			wrap_occured = true;
		x = cols - 1;
	}
	cursor.x = x;

	y = (y < 0) ? 0:
		(y >= lines) ? lines - 1: y;

	if (cursor.y == top && y_offset < 0) {
		y = top;
		scroll(top, bottom, y_offset);
	} else if (cursor.y == bottom && y_offset > 0) {
		y = bottom;
		scroll(top, bottom, y_offset);
	}
	cursor.y = y;

	if (y_offset > 0 && !nlseen) {
		txt.push("");
		lines_available++;
	}
}

/* absolute movement: never scroll */
void YaFT_p::set_cursor(int y, int x)
{
	int top, bottom;

	if (mode & MODE_ORIGIN) {
		top    = scrollm.top;
		bottom = scrollm.bottom;
		y += scrollm.top;
	} else {
		top = 0;
		bottom = lines - 1;
	}

	x = (x < 0) ? 0: (x >= cols) ? cols - 1: x;
	y = (y < top) ? top: (y > bottom) ? bottom: y;

	if (cursor.y != y && !nlseen) {
		txt.push("");
		lines_available++;
	}

	cursor.x = x;
	cursor.y = y;
	wrap_occured = false;
}

void YaFT_p::addch(uint32_t code)
{
	int _width;
	const struct glyph_t *glyphp;

	logging(DEBUG, "addch: U+%.4X\n", code);

	_width = wcwidth(code);

	if (code <= 0xff) { /* non-ascii not supported */
		char c = (char)code;
		txt.back().push_back(c);
	}
	if (_width <= 0)                               /* zero width: not support comibining character */
		return;
	else if (0x100000 <= code && code <= 0x10FFFD) /* unicode private area: plane 16 (DRCSMMv1) */
		glyphp = glyph[SUBSTITUTE_HALF];
	else if (code >= UCS2_CHARS              /* yaft support only UCS2 */
		|| glyph[code] == NULL           /* missing glyph */
		|| glyph[code]->width != _width) /* width unmatch */
		glyphp = (_width == 1) ? glyph[SUBSTITUTE_HALF] : glyph[SUBSTITUTE_WIDE];
	else
		glyphp = glyph[code];

	if ((wrap_occured && cursor.x == cols - 1) /* folding */
		|| (glyphp->width == WIDE && cursor.x == cols - 1)) {
		set_cursor(cursor.y, 0);
		move_cursor(1, 0);
	}
	wrap_occured = false;

	move_cursor(0, set_cell(cursor.y, cursor.x, glyphp));
}

void YaFT_p::reset_esc(void)
{
	logging(DEBUG, "*esc reset*\n");

	esc.buf.clear();
	esc.bp    = 0;
	esc.state = STATE_RESET;
}

bool YaFT_p::push_esc(uint8_t ch)
{
	/* ref: http://www.vt100.net/docs/vt102-ug/appendixd.html */
	esc.bp++;
	esc.buf.push_back(ch);
	if (esc.state == STATE_ESC) {
		/* format:
			ESC  I.......I F
				 ' '  '/'  '0'  '~'
			0x1B 0x20-0x2F 0x30-0x7E
		*/
		if ('0' <= ch && ch <= '~')        /* final char */
			return true;
		else if (SPACE <= ch && ch <= '/') /* intermediate char */
			return false;
	} else if (esc.state == STATE_CSI) {
		/* format:
			CSI       P.......P I.......I F
			ESC  '['  '0'  '?'  ' '  '/'  '@'  '~'
			0x1B 0x5B 0x30-0x3F 0x20-0x2F 0x40-0x7E
		*/
		if ('@' <= ch && ch <= '~')
			return true;
		else if (SPACE <= ch && ch <= '?')
			return false;
	} else {
		/* format:
			OSC       I.....I F
			ESC  ']'          BEL  or ESC  '\'
			0x1B 0x5D unknown 0x07 or 0x1B 0x5C
			DCS       I....I  F
			ESC  'P'          BEL  or ESC  '\'
			0x1B 0x50 unknown 0x07 or 0x1B 0x5C
		*/
		if (ch == BEL || (ch == BACKSLASH
			&& esc.bp >= 2 && esc.buf[esc.bp-2] == ESC))
			return true;
		else if ((ch == ESC || ch == CR || ch == LF || ch == BS || ch == HT)
			|| (SPACE <= ch && ch <= '~'))
			return false;
	}

	/* invalid sequence */
	reset_esc();
	return false;
}

void YaFT_p::reset_charset(void)
{
	charset.code = charset.count = charset.following_byte = 0;
	charset.is_valid = true;
}

void YaFT_p::reset(void)
{
	mode  = MODE_RESET;
	mode |= MODE_AMRIGHT; //(MODE_CURSOR | MODE_AMRIGHT);
	wrap_occured = false;

	scrollm.top    = 0;
	scrollm.bottom = lines - 1;

	cursor.x = cursor.y = 0;

	state.mode      = mode;
	state.cursor    = cursor;
	state.attribute = ATTR_RESET;

	color_pair.fg = DEFAULT_FG;
	color_pair.bg = DEFAULT_BG;

	attribute = ATTR_RESET;

	for (int line = 0; line < lines; line++) {
		for (int col = 0; col < cols; col++) {
			erase_cell(line, col);
			if ((col % TABSTOP) == 0)
				tabstop[col] = true;
			else
				tabstop[col] = false;
		}
		line_dirty[line] = true;
	}

	reset_esc();
	reset_charset();
}

void YaFT_p::term_die(void)
{
	line_dirty.clear();
	tabstop.clear();
	esc.buf.clear();
	cells.clear();
}

bool YaFT_p::term_init(int w, int h)
{
	const glyph_t *_glyphs;

	width  = w;
	height = h;

	int j = 0;
	do {
		_glyphs = glyphs[j];
		CELL_WIDTH = _glyphs[0].code;
		CELL_HEIGHT = _glyphs[0].width;
		cols  = width / CELL_WIDTH;
		if (cols > 79)
			break;
		j++;
	} while (glyphs[j]);

	lines = height / CELL_HEIGHT;

	logging(NORMAL, "terminal cols:%d lines:%d\n", cols, lines);

	/* allocate memory */
	line_dirty.reserve(lines);
	tabstop.reserve(cols);
	esc.buf.reserve(1024);

	cells.clear();
	std::vector<cell_t> line;
	line.resize(cols);
	for (int i = 0; i < lines; i++)
		cells.push_back(line);
	line.resize(0);

	/* initialize palette */
	for (int i = 0; i < COLORS; i++)
		virtual_palette[i] = color_list[i];
	palette_modified = true; /* first refresh() will initialize real_palette[] */

	/* initialize glyph map */
	for (uint32_t code = 0; code < UCS2_CHARS; code++)
		glyph[code] = NULL;

	for (uint32_t gi = 1; _glyphs[gi].code > 0; gi++)
		glyph[_glyphs[gi].code] = &_glyphs[gi];

	if (!glyph[DEFAULT_CHAR]
		|| !glyph[SUBSTITUTE_HALF]
		|| !glyph[SUBSTITUTE_WIDE]) {
		logging(NORMAL, "couldn't find essential glyph:\
			DEFAULT_CHAR(U+%.4X):%p SUBSTITUTE_HALF(U+%.4X):%p SUBSTITUTE_WIDE(U+%.4X):%p\n",
			DEFAULT_CHAR, glyph[DEFAULT_CHAR],
			SUBSTITUTE_HALF, glyph[SUBSTITUTE_HALF],
			SUBSTITUTE_WIDE, glyph[SUBSTITUTE_WIDE]);
		return false;
	}

	/* reset terminal */
	reset();

	return true;
}

void YaFT_p::parse(uint8_t *buf, int size)
{
	/*
		CTRL CHARS      : 0x00 ~ 0x1F
		ASCII(printable): 0x20 ~ 0x7E
		CTRL CHARS(DEL) : 0x7F
		UTF-8           : 0x80 ~ 0xFF
	*/
	uint8_t ch;

	for (int i = 0; i < size; i++) {
		ch = buf[i];
		if (esc.state == STATE_RESET) {
			/* interrupted by illegal byte */
			if (charset.following_byte > 0 && (ch < 0x80 || ch > 0xBF)) {
				addch(REPLACEMENT_CHAR);
				reset_charset();
			}

			if (ch <= 0x1F)
				control_character(ch);
			else if (ch <= 0x7F)
				addch(ch);
			else
				utf8_charset(ch);
		} else if (esc.state == STATE_ESC) {
			if (push_esc(ch))
				esc_sequence(ch);
		} else if (esc.state == STATE_CSI) {
			if (push_esc(ch))
				csi_sequence(ch);
		} else if (esc.state == STATE_OSC) {
			if (push_esc(ch))
				osc_sequence(ch);
		}
	}
}

/* ctr char/esc sequence/charset function */
void YaFT_p::control_character(uint8_t ch)
{
	static const char *ctrl_char[] = {
		"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
		"BS ", "HT ", "LF ", "VT ", "FF ", "CR ", "SO ", "SI ",
		"DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
		"CAN", "EM ", "SUB", "ESC", "FS ", "GS ", "RS ", "US ",
	};

	logging(DEBUG, "ctl: %s\n", ctrl_char[ch]);

	switch(ch) {
	case BS:  bs(); break;
	case HT:  tab(); break;
	case LF:  nlseen = true;
		  nl(); break;
	case VT:  nl(); break;
	case FF:  nl(); break;
	case CR:  cr(); break;
	case ESC: enter_esc(); break;
	default: break;
	}
}

void YaFT_p::esc_sequence(uint8_t ch)
{
	logging(DEBUG, "esc: ESC %s\n", esc.buf.c_str());

	if (esc.bp == 1)
	{
		switch(ch) {
		case '7': save_state();		break;
		case '8': restore_state();	break;
		case 'D': nl();			break;
		case 'E': crnl();		break;
		case 'H': set_tabstop();	break;
		case 'M': reverse_nl();		break;
		case 'Z': identify();		break;
		case '[': enter_csi();		break;
		case ']': enter_osc();		break;
		case 'c': ris();		break;
		default:			break;
		}
	}

	/* not reset if csi/osc/dcs seqence */
	if (ch == '[' || ch == ']' || ch == 'P')
		return;

	reset_esc();
}

void YaFT_p::csi_sequence(uint8_t ch)
{
	struct parm_t parm;

	esc.buf.resize(esc.bp - 1);
	std::string csi = esc.buf.substr(1);
	logging(DEBUG, "csi: CSI %s\n", csi.c_str());

	reset_parm(&parm);
	parse_arg(csi, &parm, ';', isdigit); /* skip '[' */

	switch (ch) {
	case '@': insert_blank(&parm);	break;
	case 'A': curs_up(&parm);	break;
	case 'B': curs_down(&parm);	break;
	case 'C': curs_forward(&parm);	break;
	case 'D': curs_back(&parm);	break;
	case 'E': curs_nl(&parm);	break;
	case 'F': curs_pl(&parm);	break;
	case 'G': curs_col(&parm);	break;
	case 'H': curs_pos(&parm);	break;
	case 'J': erase_display(&parm);	break;
	case 'K': erase_line(&parm);	break;
	case 'L': insert_line(&parm);	break;
	case 'M': delete_line(&parm);	break;
	case 'P': delete_char(&parm);	break;
	case 'X': erase_char(&parm);	break;
	case 'a': curs_forward(&parm);	break;
	case 'c': device_attribute(&parm);break;
	case 'd': curs_line(&parm);	break;
	case 'e': curs_down(&parm);	break;
	case 'f': curs_pos(&parm);	break;
	case 'g': clear_tabstop(&parm);	break;
	case 'h': set_mode(&parm);	break;
	case 'l': reset_mode(&parm);	break;
	case 'm': set_attr(&parm);	break;
	case 'n': status_report(&parm);	break;
	case 'r': set_margin(&parm);	break;
	/* XXX: not implemented because these sequences conflict DECSLRM/DECSHTS
	case 's': sco_save_state(&parm);break;
	case 'u': sco_restore_state(&parm); break;
	*/
	case '`': curs_col(&parm);	break;
	default: break;
	}

	reset_esc();
}

static int is_osc_parm(int c)
{
	if (isdigit(c) || isalpha(c) ||
		c == '?' || c == ':' || c == '/' || c == '#')
		return true;
	else
		return false;
}

#if 0
static void omit_string_terminator(char *bp, uint8_t ch)
{
	if (ch == BACKSLASH) /* ST: ESC BACKSLASH */
		*(bp - 2) = '\0';
	else                 /* ST: BEL */
		*(bp - 1) = '\0';
}
#endif

void YaFT_p::osc_sequence(uint8_t ch)
{
	int osc_mode;
	struct parm_t parm;

	if (ch == BACKSLASH) /* ST: ESC BACKSLASH */
		esc.buf.resize(esc.bp - 2);
	else                 /* ST: BEL */
		esc.buf.resize(esc.bp - 1);
	// omit_string_terminator(esc.bp, ch);

	logging(DEBUG, "osc: OSC %s\n", esc.buf.c_str());

	std::string osc = esc.buf.substr(1);
	reset_parm(&parm);
	parse_arg(osc, &parm, ';', is_osc_parm); /* skip ']' */

	if (parm.argc > 0) {
		osc_mode = dec2num(parm.argv[0]);
		logging(DEBUG, "osc_mode:%d\n", osc_mode);

		/* XXX: disable because this functions only work 24/32bpp
			-> support other bpp (including pseudo color) */
		if (osc_mode == 4)
			set_palette(&parm);
		else if (osc_mode == 104)
			reset_palette(&parm);
#if 0
		if (osc_mode == 8900)
			glyph_width_report(&parm);
#endif
	}
	reset_esc();
}

void YaFT_p::utf8_charset(uint8_t ch)
{
	if (0x80 <= ch && ch <= 0xBF) {
		/* check illegal UTF-8 sequence
			* ? byte sequence: first byte must be between 0xC2 ~ 0xFD
			* 2 byte sequence: first byte must be between 0xC2 ~ 0xDF
			* 3 byte sequence: second byte following 0xE0 must be between 0xA0 ~ 0xBF
			* 4 byte sequence: second byte following 0xF0 must be between 0x90 ~ 0xBF
			* 5 byte sequence: second byte following 0xF8 must be between 0x88 ~ 0xBF
			* 6 byte sequence: second byte following 0xFC must be between 0x84 ~ 0xBF
		*/
		if ((charset.following_byte == 0)
			|| (charset.following_byte == 1 && charset.count == 0 && charset.code <= 1)
			|| (charset.following_byte == 2 && charset.count == 0 && charset.code == 0 && ch < 0xA0)
			|| (charset.following_byte == 3 && charset.count == 0 && charset.code == 0 && ch < 0x90)
			|| (charset.following_byte == 4 && charset.count == 0 && charset.code == 0 && ch < 0x88)
			|| (charset.following_byte == 5 && charset.count == 0 && charset.code == 0 && ch < 0x84))
			charset.is_valid = false;

		charset.code <<= 6;
		charset.code += ch & 0x3F;
		charset.count++;
	} else if (0xC0 <= ch && ch <= 0xDF) {
		charset.code = ch & 0x1F;
		charset.following_byte = 1;
		charset.count = 0;
		return;
	} else if (0xE0 <= ch && ch <= 0xEF) {
		charset.code = ch & 0x0F;
		charset.following_byte = 2;
		charset.count = 0;
		return;
	} else if (0xF0 <= ch && ch <= 0xF7) {
		charset.code = ch & 0x07;
		charset.following_byte = 3;
		charset.count = 0;
		return;
	} else if (0xF8 <= ch && ch <= 0xFB) {
		charset.code = ch & 0x03;
		charset.following_byte = 4;
		charset.count = 0;
		return;
	} else if (0xFC <= ch && ch <= 0xFD) {
		charset.code = ch & 0x01;
		charset.following_byte = 5;
		charset.count = 0;
		return;
	} else { /* 0xFE - 0xFF: not used in UTF-8 */
		addch(REPLACEMENT_CHAR);
		reset_charset();
		return;
	}

	if (charset.count >= charset.following_byte) {
		/*	illegal code point (ref: http://www.unicode.org/reports/tr27/tr27-4.html)
			0xD800   ~ 0xDFFF : surrogate pair
			0xFDD0   ~ 0xFDEF : noncharacter
			0xnFFFE  ~ 0xnFFFF: noncharacter (n: 0x00 ~ 0x10)
			0x110000 ~        : invalid (unicode U+0000 ~ U+10FFFF)
		*/
		if (!charset.is_valid
			|| (0xD800 <= charset.code && charset.code <= 0xDFFF)
			|| (0xFDD0 <= charset.code && charset.code <= 0xFDEF)
			|| ((charset.code & 0xFFFF) == 0xFFFE || (charset.code & 0xFFFF) == 0xFFFF)
			|| (charset.code > 0x10FFFF))
			addch(REPLACEMENT_CHAR);
		else
			addch(charset.code);

		reset_charset();
	}
}

int YaFT_p::sum(struct parm_t *parm)
{
	int s = 0;
	for (int i = 0; i < parm->argc; i++)
		s += dec2num(parm->argv[i]);
	return s;
}

static int my_ceil(int val, int div)
{
	if (div == 0)
		return 0;
	else
		return (val + div - 1) / div;
}

void YaFT_p::draw_line(int line)
{
	int pos, bdf_padding, glyph_w, margin_right;
	int col, w, h;
	uint32_t pixel;
	struct color_pair_t col_pair;
	struct cell_t *cellp;

	if (fb.dy_min > line * CELL_HEIGHT)
		fb.dy_min = line * CELL_HEIGHT;
	if (fb.dy_max < (line+1) * CELL_HEIGHT - 1)
		fb.dy_max = (line+1) * CELL_HEIGHT - 1;

	//std::string s = "";
	for (col = cols - 1; col >= 0; col--) {
		margin_right = (cols - 1 - col) * CELL_WIDTH;

		/* target cell */
		cellp = &cells[line][col];

		/* copy current color_pair (maybe changed) */
		col_pair = cellp->color_pair;

		/* check wide character or not */
		glyph_w = CELL_WIDTH;
		bdf_padding = my_ceil(glyph_w, BITS_PER_BYTE) * BITS_PER_BYTE - glyph_w;

		/* check cursor positon */
		if ((mode & MODE_CURSOR && line == cursor.y)
			&& col == cursor.x) {
			col_pair.fg = DEFAULT_BG;
			col_pair.bg = ACTIVE_CURSOR_COLOR;
		}

		for (h = 0; h < CELL_HEIGHT; h++) {
			/* if UNDERLINE attribute on, swap bg/fg */
			if ((h == (CELL_HEIGHT - 1)) && (cellp->attribute & attr_mask[ATTR_UNDERLINE]))
				col_pair.bg = col_pair.fg;

			pos = (width - 1 - margin_right/* - w*/)
				+ (line * CELL_HEIGHT + h) * fb.width;

			for (w = 0; w < CELL_WIDTH; w++) {
				/* set color palette */
				if (cellp->glyphp->bitmap[h] & (0x01 << (bdf_padding + w)))
					pixel = fb.real_palette[col_pair.fg];
				else
					pixel = fb.real_palette[col_pair.bg];

				/* update copy buffer only */
				//memcpy(fb.buf + pos, &pixel, fb.info.bytes_per_pixel);
				fb.buf[pos] = pixel;
				pos--;
			}
		}
		//s.insert(s.begin(), cellp->glyphp->code);
	}
	//printf("draw_line: %02d ",  line);puts(s.c_str());

	line_dirty[line] = ((mode & MODE_CURSOR) && cursor.y == line) ? true: false;
}

static inline int color2pixel(uint32_t color, struct fb_var_screeninfo *var)
{
	uint32_t r, g, b;
	r = (color >> 16) & 0x000000ff;
	g = (color >> 8)  & 0x000000ff;
	b = (color >> 0)  & 0x000000ff;
	return (0xff << var->transp.offset) |
		(r << var->red.offset) |
		(g << var->green.offset) |
		(b << var->blue.offset);
}

/* actually update the framebuffer contents */
void YaFT_p::refresh()
{
	if (! paint)
		return;

	if (palette_modified) {
		palette_modified = false;
		for (int i = 0; i < COLORS; i++)
			fb.real_palette[i] = color2pixel(virtual_palette[i], screeninfo);
	}

	logging(DEBUG,"%s: mode %x cur: %x cursor.y %d\n", __func__, mode, MODE_CURSOR, cursor.y);
	if (mode & MODE_CURSOR)
		line_dirty[cursor.y] = true;

	for (int line = 0; line < lines; line++) {
		if (line_dirty[line]) {
			draw_line(line);
		}
	}
#if 1
	if (fb.dy_max != -1) {
		int blit_height = fb.dy_max - fb.dy_min;
		uint32_t *blit_start = fb.buf + (fb.dy_min * fb.width);
		fb.cfb->blit2FB(blit_start, fb.width, blit_height, fb.xstart, fb.ystart+fb.dy_min, 0, 0);
	}
#else
	fb.cfb->blit2FB(fb.buf, fb.width, fb.height, fb.xstart, fb.ystart, 0, 0);
#endif
	fb.dy_min = fb.height;
	fb.dy_max = -1;
	last_paint = time_monotonic_ms();
}
