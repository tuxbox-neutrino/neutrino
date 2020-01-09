/*
 * yaft framebuffer terminal as C++ class for embedding in neutrino-MP
 * (C) 2018 Stefan Seyfried
 * License: GPL-2.0
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * derived from yaft/ctrlseq/csi.h
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
#include <cstring>

/* function for csi sequence */
void YaFT_p::insert_blank(struct parm_t *parm)
{
	int i, num = sum(parm);

	if (num <= 0)
		num = 1;

	for (i = cols - 1; cursor.x <= i; i--) {
		if (cursor.x <= (i - num))
			copy_cell(cursor.y, i, cursor.y, i - num);
		else
			erase_cell(cursor.y, i);
	}
}

void YaFT_p::curs_up(struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(-num, 0);
}

void YaFT_p::curs_down(struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(num, 0);
}

void YaFT_p::curs_forward(struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(0, num);
}

void YaFT_p::curs_back(struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(0, -num);
}

void YaFT_p::curs_nl(struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(num, 0);
	cr();
}

void YaFT_p::curs_pl(struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(-num, 0);
	cr();
}

void YaFT_p::curs_col(struct parm_t *parm)
{
	int num;

	num = (parm->argc <= 0) ? 0 : dec2num(parm->argv[parm->argc - 1]) - 1;
	set_cursor(cursor.y, num);
}

void YaFT_p::curs_pos(struct parm_t *parm)
{
	int line, col;

	if (parm->argc <= 0) {
		line = col = 0;
	} else if (parm->argc == 2) {
		line = dec2num(parm->argv[0]) - 1;
		col  = dec2num(parm->argv[1]) - 1;
	} else {
		return;
	}

	if (line < 0)
		line = 0;
	if (col < 0)
		col = 0;

	set_cursor(line, col);
}

void YaFT_p::curs_line(struct parm_t *parm)
{
	int num;

	num = (parm->argc <= 0) ? 0 : dec2num(parm->argv[parm->argc - 1]) - 1;
	set_cursor(num, cursor.x);
}

void YaFT_p::erase_display(struct parm_t *parm)
{
	int i, j, pmode;

	pmode = (parm->argc <= 0) ? 0 : dec2num(parm->argv[parm->argc - 1]);

	if (pmode < 0 || 2 < pmode)
		return;

	if (pmode == 0) {
		for (i = cursor.y; i < lines; i++)
			for (j = 0; j < cols; j++)
				if (i > cursor.y || (i == cursor.y && j >= cursor.x))
					erase_cell(i, j);
	} else if (pmode == 1) {
		for (i = 0; i <= cursor.y; i++)
			for (j = 0; j < cols; j++)
				if (i < cursor.y || (i == cursor.y && j <= cursor.x))
					erase_cell(i, j);
	} else if (pmode == 2) {
		for (i = 0; i < lines; i++)
			for (j = 0; j < cols; j++)
				erase_cell(i, j);
	}
}

void YaFT_p::erase_line(struct parm_t *parm)
{
	int i, pmode;

	pmode = (parm->argc <= 0) ? 0 : dec2num(parm->argv[parm->argc - 1]);

	if (pmode < 0 || 2 < pmode)
		return;

	if (pmode == 0) {
		for (i = cursor.x; i < cols; i++)
			erase_cell(cursor.y, i);
	} else if (pmode == 1) {
		for (i = 0; i <= cursor.x; i++)
			erase_cell(cursor.y, i);
	} else if (pmode == 2) {
		for (i = 0; i < cols; i++)
			erase_cell(cursor.y, i);
	}
}

void YaFT_p::insert_line(struct parm_t *parm)
{
	int num = sum(parm);

	if (mode & MODE_ORIGIN) {
		if (cursor.y < scrollm.top
			|| cursor.y > scrollm.bottom)
			return;
	}

	if (num <= 0)
		num = 1;

	scroll(cursor.y, scrollm.bottom, -num);
}

void YaFT_p::delete_line(struct parm_t *parm)
{
	int num = sum(parm);

	if (mode & MODE_ORIGIN) {
		if (cursor.y < scrollm.top
			|| cursor.y > scrollm.bottom)
			return;
	}

	if (num <= 0)
		num = 1;

	scroll(cursor.y, scrollm.bottom, num);
}

void YaFT_p::delete_char(struct parm_t *parm)
{
	int i, num = sum(parm);

	if (num <= 0)
		num = 1;

	for (i = cursor.x; i < cols; i++) {
		if ((i + num) < cols)
			copy_cell(cursor.y, i, cursor.y, i + num);
		else
			erase_cell(cursor.y, i);
	}
}

void YaFT_p::erase_char(struct parm_t *parm)
{
	int i, num = sum(parm);

	if (num <= 0)
		num = 1;
	else if (num + cursor.x > cols)
		num = cols - cursor.x;

	for (i = cursor.x; i < cursor.x + num; i++)
		erase_cell(cursor.y, i);
}

void YaFT_p::set_attr(struct parm_t *parm)
{
	int i, num;

	if (parm->argc <= 0) {
		attribute     = ATTR_RESET;
		color_pair.fg = DEFAULT_FG;
		color_pair.bg = DEFAULT_BG;
		return;
	}

	for (i = 0; i < parm->argc; i++) {
		num = dec2num(parm->argv[i]);

		if (num == 0) {                        /* reset all attribute and color */
			attribute     = ATTR_RESET;
			color_pair.fg = DEFAULT_FG;
			color_pair.bg = DEFAULT_BG;
		} else if (1 <= num && num <= 7) {     /* set attribute */
			attribute |= attr_mask[num];
		} else if (21 <= num && num <= 27) {   /* reset attribute */
			attribute &= ~attr_mask[num - 20];
		} else if (30 <= num && num <= 37) {   /* set foreground */
			color_pair.fg = (num - 30);
		} else if (num == 38) {                /* set 256 color to foreground */
			if ((i + 2) < parm->argc && dec2num(parm->argv[i + 1]) == 5) {
				color_pair.fg = dec2num(parm->argv[i + 2]);
				i += 2;
			}
		} else if (num == 39) {                /* reset foreground */
			color_pair.fg = DEFAULT_FG;
		} else if (40 <= num && num <= 47) {   /* set background */
			color_pair.bg = (num - 40);
		} else if (num == 48) {                /* set 256 color to background */
			if ((i + 2) < parm->argc && dec2num(parm->argv[i + 1]) == 5) {
				color_pair.bg = dec2num(parm->argv[i + 2]);
				i += 2;
			}
		} else if (num == 49) {                /* reset background */
			color_pair.bg = DEFAULT_BG;
		} else if (90 <= num && num <= 97) {   /* set bright foreground */
			color_pair.fg = (num - 90) + BRIGHT_INC;
		} else if (100 <= num && num <= 107) { /* set bright background */
			color_pair.bg = (num - 100) + BRIGHT_INC;
		}
	}
}

void YaFT_p::status_report(struct parm_t *parm)
{
	int i, num;
	char buf[BUFSIZE];
	ssize_t ignored __attribute__((unused));

	for (i = 0; i < parm->argc; i++) {
		num = dec2num(parm->argv[i]);
		if (num == 5) {         /* terminal response: ready */
			ignored = write(fd, "\033[0n", 4);
		} else if (num == 6) {  /* cursor position report */
			snprintf(buf, BUFSIZE, "\033[%d;%dR", cursor.y + 1, cursor.x + 1);
			ignored = write(fd, buf, strlen(buf));
		} else if (num == 15) { /* terminal response: printer not connected */
			ignored = write(fd, "\033[?13n", 6);
		}
	}
}

void YaFT_p::device_attribute(struct parm_t *parm)
{
	if (parm->argc > 0 && dec2num(parm->argv[0]))
		return; /* compatibility with linux console */
	ssize_t ignored __attribute__((unused)) = write(fd, "\033[?6c", 5); /* "I am a VT102" */
}

void YaFT_p::set_mode(struct parm_t *parm)
{
	int i, pmode;

	for (i = 0; i < parm->argc; i++) {
		pmode = dec2num(parm->argv[i]);
		if (esc.buf[1] != '?')
			continue; /* not supported */

		if (pmode == 6) { /* private mode */
			mode |= MODE_ORIGIN;
			set_cursor(0, 0);
		} else if (pmode == 7) {
			mode |= MODE_AMRIGHT;
		} else if (pmode == 25) {
			mode |= MODE_CURSOR;
		}
	}

}

void YaFT_p::reset_mode(struct parm_t *parm)
{
	int i, pmode;

	for (i = 0; i < parm->argc; i++) {
		pmode = dec2num(parm->argv[i]);
		if (esc.buf[1] != '?')
			continue; /* not supported */

		if (pmode == 6) { /* private mode */
			mode &= ~MODE_ORIGIN;
			set_cursor(0, 0);
		} else if (pmode == 7) {
			mode &= ~MODE_AMRIGHT;
			wrap_occured = false;
		} else if (pmode == 25) {
			mode &= ~MODE_CURSOR;
		}
	}

}

void YaFT_p::set_margin(struct parm_t *parm)
{
	int top, bottom;

	if (parm->argc <= 0) {        /* CSI r */
		top    = 0;
		bottom = lines - 1;
	} else if (parm->argc == 2) { /* CSI ; r -> use default value */
		top    = parm->argv[0].empty() ? 0 : dec2num(parm->argv[0]) - 1;
		bottom = parm->argv[1].empty() ? lines - 1 : dec2num(parm->argv[1]) - 1;
	} else {
		return;
	}

	if (top < 0 || top >= lines)
		top = 0;
	if (bottom < 0 || bottom >= lines)
		bottom = lines - 1;

	if (top >= bottom)
		return;

	scrollm.top = top;
	scrollm.bottom = bottom;

	set_cursor(0, 0); /* move cursor to home */
}

void YaFT_p::clear_tabstop(struct parm_t *parm)
{
	int i, j, num;

	if (parm->argc <= 0) {
		tabstop[cursor.x] = false;
	} else {
		for (i = 0; i < parm->argc; i++) {
			num = dec2num(parm->argv[i]);
			if (num == 0) {
				tabstop[cursor.x] = false;
			} else if (num == 3) {
				for (j = 0; j < cols; j++)
					tabstop[j] = false;
				return;
			}
		}
	}
}
