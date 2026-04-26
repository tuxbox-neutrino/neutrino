/*
 * Private backend declarations for the YAFT framebuffer terminal integration
 * in Neutrino.
 * Copyright (C) 2018 Stefan Seyfried
 *
 * Neutrino integration license: GPL-2.0-only.
 *
 * This file contains declarations derived from upstream yaft
 * (https://github.com/uobikiemukot/yaft), originally:
 * Copyright (c) 2012 haru <uobikiemukot at gmail dot com>
 *
 * Upstream yaft is MIT-licensed; see LICENSE in this directory for the
 * original permission notice and warranty disclaimer. This integration is
 * distributed with Neutrino under the GPL terms above, without warranty.
 */

#ifndef YAFT_PRIV_H
#define YAFT_PRIV_H

#include <array>
#include <cstddef>
#include <queue>
#include <map>
#include <string>
#include <stdint.h>
#include <stdlib.h> /* atoi(), calloc(), free(), strtol() */
#include <unistd.h> /* write() */
#include <stdio.h>

#include "color.h"

#include <system/debug.h>
#include <driver/fb_generic.h>
#include <driver/fontrenderer.h>

constexpr size_t BUFSIZE = 1024;
constexpr size_t MAX_ARGS = 16;

#define logging(a, message...) dprintf(DEBUG_ ## a, "YaFT: " message)

constexpr std::array<uint8_t, 8> attr_mask = {
	0x00, 0x01, 0x00, 0x00, /* 0:none      1:bold  2:none 3:none */
	0x02, 0x04, 0x00, 0x08  /* 4:underline 5:blink 6:none 7:reverse */
};

struct parm_t { /* for parse_arg() */
	int argc;
	std::string argv[MAX_ARGS];
};

enum char_code {
	/* 7 bit */
	BEL = 0x07, BS  = 0x08, HT  = 0x09,
	LF  = 0x0A, VT  = 0x0B, FF  = 0x0C,
	CR  = 0x0D, ESC = 0x1B, DEL = 0x7F,
	/* others */
	SPACE     = 0x20,
	BACKSLASH = 0x5C,
};

class YaFT_p
{
	/* color: index number of color_palette[] (see color.h) */
	enum {
		DEFAULT_FG           = 7,
		DEFAULT_BG           = 0,
		ACTIVE_CURSOR_COLOR  = 2,
		PASSIVE_CURSOR_COLOR = 1,
	};

	/* misc */
	enum {
		TABSTOP          = 8,      /* hardware tabstop */
		SUBSTITUTE_HALF  = 0x0020, /* used for missing glyph(single width): U+0020 (SPACE) */
		SUBSTITUTE_WIDE  = 0x3000, /* used for missing glyph(double width): U+3000 (IDEOGRAPHIC SPACE) */
		REPLACEMENT_CHAR = 0x003F, /* used for malformed UTF-8 sequence   : U+003F (QUESTION MARK) */
	};

	enum misc {
		BITS_PER_BYTE = 8,         /* bits per byte */
		UCS2_CHARS    = 0x10000,   /* number of UCS2 glyphs */
		DEFAULT_CHAR  = SPACE,     /* used for erase char */
		BRIGHT_INC    = 8,         /* value used for brightening color */
	};

	struct margin_t { uint16_t top, bottom; };
	struct point_t { uint16_t x, y; };
	struct color_pair_t { uint8_t fg, bg; };

	struct state_t {   /* for save, restore state */
		struct point_t cursor;
		int mode;
		int attribute;
	};

	struct charset_t {
		uint32_t code; /* UCS4 code point: yaft only prints UCS2 and DRCSMMv1 */
		int following_byte, count;
		bool is_valid;
	};

	enum esc_state {
		STATE_RESET = 0x00,
		STATE_ESC   = 0x01, /* 0x1B, \033, ESC */
		STATE_CSI   = 0x02, /* ESC [ */
		STATE_OSC   = 0x04, /* ESC ] */
	};

	enum char_attr {
		ATTR_RESET     = 0,
		ATTR_BOLD      = 1, /* brighten foreground */
		ATTR_UNDERLINE = 4,
		ATTR_BLINK     = 5, /* brighten background */
		ATTR_REVERSE   = 7,
	};

	enum term_mode {
		MODE_RESET   = 0x00,
		MODE_ORIGIN  = 0x01, /* origin mode: DECOM */
		MODE_CURSOR  = 0x02, /* cursor visible: DECTCEM */
		MODE_AMRIGHT = 0x04, /* auto wrap: DECAWM */
	};

	struct esc_t {
		std::string buf;
		int bp; /* buffer position */
		enum esc_state state;
	};

	struct cell_t {
		struct color_pair_t color_pair; /* color (fg, bg) */
		int attribute;                  /* bold, underscore, etc... */
		std::string utf8_str;
	};

	struct framebuffer_t {
		int fd;                        /* file descriptor of framebuffer */
		uint32_t *buf;                 /* compact YAFT surface buffer */
		uint32_t real_palette[COLORS]; /* hardware specific color palette */
		int width, height;             /* display resolution */
		int xstart, ystart;            /* position of the window in the framebuffer */
		long screen_size;              /* screen data size (byte) */
		int line_length;               /* line length (byte) */
		int dy_min, dy_max;            /* dirty region for blit */
		CFrameBuffer *cfb;
	};

 private:
	int width, height;                       /* terminal size (pixel) */
	std::vector<std::vector<cell_t> > cells;
	struct margin_t scrollm;                 /* scroll margin */
	struct point_t cursor;                   /* cursor pos (x, y) */
	std::vector<bool> line_dirty;            /* dirty flag */
	std::vector<bool> tabstop;               /* tabstop flag */
	int mode;                                /* for set/reset mode */
	bool wrap_occured;                       /* whether auto wrap occured or not */
	struct state_t state;                    /* for restore */
	struct color_pair_t color_pair;          /* color (fg, bg) */
	int attribute;                           /* bold, underscore, etc... */
	struct charset_t charset;                /* store UTF-8 byte stream */
	struct esc_t esc;                        /* store escape sequence */
	uint32_t virtual_palette[COLORS];        /* virtual color palette: always 32bpp */
	bool palette_modified;                   /* true if palette changed by OSC 4/104 */
	bool nlseen;
	int CELL_WIDTH, CELL_HEIGHT;
	struct framebuffer_t fb;
	struct fb_var_screeninfo *screeninfo;
	bool paint;
	FBFontRenderClass *fr;
	Font *font;
	const char *fontstyle;
 public:
	int fd;                                  /* master of pseudo terminal */
	int cols, lines;                         /* terminal size (cell) */
	time_t last_paint;
	std::queue<std::string> txt;             /* contains sanitized output text */

	YaFT_p(bool paint = true);
	~YaFT_p();
	bool init();
	void parse(uint8_t *buf, int size);
	void refresh(void);
 private:
	void draw_line(int line);
	void erase_cell(int y, int x);
	void copy_cell(int dst_y, int dst_x, int src_y, int src_x);
	int set_cell(int y, int x, std::string &utf);
	void swap_lines(int i, int j);
	void scroll(int from, int to, int offset);
	void move_cursor(int y_offset, int x_offset);
	void set_cursor(int y, int x);
	void addch(uint32_t code);
	bool push_esc(uint8_t ch);
	void reset_esc(void);
	void reset_charset(void);
	void reset(void);
	void term_die(void);
	void utf8_charset(uint8_t ch);
	void control_character(uint8_t ch);
	void esc_sequence(uint8_t ch);
	void csi_sequence(uint8_t ch);
	void osc_sequence(uint8_t ch);
	void bs(void);
	void nl(void);
	void cr(void);
	void ris(void);
	void crnl(void);
	void curs_up(parm_t *parm);
	void curs_down(parm_t *parm);
	void curs_back(parm_t *parm);
	void curs_forward(parm_t *parm);
	void curs_nl(parm_t *parm);
	void curs_pl(parm_t *parm);
	void curs_col(parm_t *parm);
	void curs_pos(parm_t *parm);
	void curs_line(parm_t *parm);
	void reverse_nl(void);
	void insert_blank(parm_t *parm);
	void erase_line(parm_t *parm);
	void insert_line(parm_t *parm);
	void delete_line(parm_t *parm);
	void erase_char(parm_t *parm);
	void delete_char(parm_t *parm);
	void device_attribute(parm_t *parm);
	void erase_display(parm_t *parm);
	void identify(void);
	void set_tabstop(void);
	void clear_tabstop(parm_t *parm);
	void set_mode(parm_t *parm);
	void reset_mode(parm_t *parm);
	void set_palette(parm_t *parm);
	void reset_palette(parm_t *parm);
	void set_attr(parm_t *parm);
	void status_report(parm_t *parm);
	void set_margin(parm_t *parm);
	void enter_esc(void);
	void enter_csi(void);
	void enter_osc(void);
	void save_state(void);
	void restore_state(void);
	void tab(void);
	int dec2num(std::string &s) { return atoi(s.c_str()); };
	int hex2num(std::string s) { return strtol(s.c_str(), NULL, 16); };
	int sum(parm_t *param);
	void parse_arg(std::string &buf, parm_t *pt, int delim, int (is_valid)(int c));
	void reset_parm(parm_t *parm);
	int32_t parse_color1(std::string &seq);
	int32_t parse_color2(std::string &seq);
};

#endif // YAFT_PRIV_H
