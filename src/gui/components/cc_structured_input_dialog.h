/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Structured input dialog
	Copyright (C) 2026

	License: GPL
*/

#ifndef __CC_STRUCTURED_INPUT_DIALOG_H__
#define __CC_STRUCTURED_INPUT_DIALOG_H__

#include "cc_input_dialog.h"

#include <time.h>
#include <vector>

/**
 * Input dialog for a value of fixed shape: an IP or MAC address, a date with
 * its time, a time of day, a weekday mask or a decimal with fixed places.
 *
 * The value is cut into segments, a field each, with the separators between
 * them as text. Every cell is always filled - with zeros where nothing was
 * typed - and typing overwrites the cell under the cursor and moves on, into
 * the next segment at the end of one: an IP address is twelve digits in a
 * row. Left and right move over all cells, up and down step the cell under
 * the cursor through what it may hold, which is also how the hex letters of
 * a MAC address are reached. Green steps back and zeroes that cell, yellow
 * zeroes all of them. On a weekday mask the digits 1 to 7 switch a day.
 *
 * Save checks every segment against its range and the whole value against
 * its format - a date has to exist. The first segment that fails takes the
 * focus, framed red, with the reason on the hint line.
 *
 * The target is a string, or a time_t for a date. For a time_t the dialog
 * keeps the text a menu row shows; getValue() returns that same string for
 * the dialog's whole life, and code that writes into it is read over at the
 * next open, which starts from the time_t again.
 */
class CCStructuredInputDialog : public CCInputDialogBase
{
	public:
		enum format_t
		{
			///"a.b.c.d", each part 0-255; all zero saves as ""
			FORMAT_IP,
			///"aa:bb:cc:dd:ee:ff", hex; all zero saves as ""
			FORMAT_MAC,
			///"dd.mm.yyyy hh:mm" of a time_t; the seconds stay as they were
			FORMAT_DATETIME,
			///"hh:mm:ss", the hours as wide as the value brings them
			FORMAT_TIME,
			///seven cells of '-' and 'X', Monday first
			FORMAT_WEEKDAYS,
			///digits, a point and a fixed number of places, see setDecimal()
			FORMAT_DECIMAL
		};

	private:
		struct segment_t
		{
			size_t len;
			std::string charset;
			bool ranged;
			long long min;
			long long max;
			std::string separator;
			std::string caption;
			CCInputBuffer buffer;
			CCInputField *field;
			CComponentsText *separator_item;
			CComponentsText *caption_item;
		};

		format_t csd_format;
		std::string *csd_value;
		time_t *csd_time;
		std::string csd_time_text;
		bool *csd_cancel;
		CChangeObserver *csd_observ;
		std::vector<segment_t *> csd_segments;
		CComponentsText *csd_hint;
		std::string csd_hint_text;
		std::string csd_error_text;
		std::string csd_captions;
		size_t csd_active;
		bool csd_saved;
		bool csd_built;
		int csd_decimal_int;
		int csd_decimal_frac;
		int csd_decimal_max;
		Font *csd_font;

		void init(format_t format);
		void defineSegments(const std::string &value);
		void addSegment(size_t len, const std::string &charset, bool ranged,
			long long min, long long max, const std::string &separator,
			const std::string &caption = std::string());
		void clearSegments();
		void buildItems();
		void layoutItems();
		std::string *getTarget();
		void loadValue(const std::string &value);
		std::string getCells() const;
		std::string composeValue() const;
		bool makeTime(time_t &result) const;
		static std::string formatTime(time_t value);
		long long segmentNumber(size_t index) const;
		bool validate(size_t &bad, std::string &reason) const;
		std::string getRangeText(const segment_t *segment) const;

		void focusSegment(size_t index, size_t cell);
		void refreshSegment(size_t index);
		void moveCursor(int delta);
		void typeGlyph(const std::string &glyph);
		void stepGlyph(int delta);
		void backspace();
		void clearCells();
		void toggleWeekday(int day);
		void showError(size_t index, const std::string &reason);
		void clearError();
		void updateHint();
		void showDisplayText();
		void stopCarets();
		bool confirmDiscard();
		bool save();

	public:
		CCStructuredInputDialog(const neutrino_locale_t title,
			format_t format,
			std::string *value,
			CChangeObserver *observ = NULL);
		///FORMAT_DATETIME for a time_t
		CCStructuredInputDialog(const neutrino_locale_t title,
			time_t *value,
			CChangeObserver *observ = NULL);
		virtual ~CCStructuredInputDialog();

		void setHintText(const std::string &hint);
		///FORMAT_DECIMAL: digits before and after the point, and the largest whole part
		void setDecimal(int int_digits, int frac_digits, int max_int);
		///set to true after exec() unless the value was saved
		void setCancelFlag(bool *cancel) { csd_cancel = cancel; }

		int exec(CMenuTarget *parent, const std::string &actionKey);
		void hide();
		std::string &getValue(void);
		///true when the dialog left through save, not through cancel, back or timeout
		bool wasSaved() const { return csd_saved; }
};

#endif
