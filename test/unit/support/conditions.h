/*
 * conditions.h - rows and lookups for cases about a row's conditions
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __test_support_conditions_h__
#define __test_support_conditions_h__

#include "coreapi/base/schema.h"

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

// A flag row that is sane apart from whatever its conditions say.
inline coreapi::Descriptor rowWithConditions(const coreapi::Condition *c, size_t count)
{
	coreapi::Descriptor d = { "k", coreapi::ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL,
				  false, false, c, count, COREAPI_NO_FIELD, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	return d;
}

/* A lookup that owns the values it answers. The context points at the object
   itself, so a copy points at its own values and never at the one it was copied
   from, which may be a temporary already gone. A key it does not hold is one it
   cannot answer for. */
class FakeLookup : public coreapi::ValueLookup
{
public:
	FakeLookup() { bind(); }
	FakeLookup(const FakeLookup &o) : numbers(o.numbers), texts(o.texts) { bind(); }
	FakeLookup &operator=(const FakeLookup &o)
	{
		numbers = o.numbers;
		texts = o.texts;
		bind();
		return *this;
	}

	std::vector<std::pair<std::string, long> > numbers;
	std::vector<std::pair<std::string, std::string> > texts;

private:
	void bind()
	{
		read = readNumber;
		read_text = readText;
		context = this;
	}

	static bool readNumber(const char *key, long *value, void *context)
	{
		const FakeLookup *self = static_cast<const FakeLookup *>(context);
		for (size_t i = 0; i < self->numbers.size(); ++i)
		{
			if (self->numbers[i].first == key)
			{
				*value = self->numbers[i].second;
				return true;
			}
		}
		return false;
	}

	static bool readText(const char *key, std::string *value, void *context)
	{
		const FakeLookup *self = static_cast<const FakeLookup *>(context);
		for (size_t i = 0; i < self->texts.size(); ++i)
		{
			if (self->texts[i].first == key)
			{
				*value = self->texts[i].second;
				return true;
			}
		}
		return false;
	}
};

inline FakeLookup textLookup(const char *key, const char *value)
{
	FakeLookup l;
	l.texts.push_back(std::make_pair(std::string(key), std::string(value)));
	return l;
}

inline FakeLookup numberLookup(std::initializer_list<std::pair<const char *, long> > values)
{
	FakeLookup l;
	for (const std::pair<const char *, long> &v : values)
		l.numbers.push_back(std::make_pair(std::string(v.first), v.second));
	return l;
}

#endif
