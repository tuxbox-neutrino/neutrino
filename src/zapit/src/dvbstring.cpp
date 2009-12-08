/*
 * $Header: /cvsroot/tuxbox/apps/dvb/zapit/src/dvbstring.cpp,v 1.1 2002/10/29 22:46:50 thegoodguy Exp $
 *
 * Strings conforming to the DVB Standard - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
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
 *
 */

#include <zapit/dvbstring.h>
#include <xmlinterface.h>

static const int ISO_6937_mapping [0xBA - 0xA8 + 1] = {0x00A4, // 0xA8
						       0x2018, // 0xA9
						       0x201C, // 0xAA
						       0x00AB, // 0xAB
						       0x2190, // 0xAC
						       0x2191, // 0xAD
						       0x2192, // 0xAE
						       0x2193, // 0xAF
						       0x00B0, // 0xB0
						       0x00B1, // 0xB1
						       0x00B2, // 0xB2
						       0x00B3, // 0xB3
						       0x00D7, // 0xB4
						       0x00B5, // 0xB5
						       0x00B6, // 0xB6
						       0x00B7, // 0xB7
						       0x00F7, // 0xB8
						       0x2019, // 0xB9
						       0x201D  // 0xBA
};

static const int ISO_6937_mapping2[0xD7 - 0xD0 + 1] = {0x2014, // 0xD0
						       0x00B9, // 0xD1
						       0x00AE, // 0xD2
						       0x00A9, // 0xD3
						       0x2122, // 0xD4
						       0x266A, // 0xD5
						       0x00AC, // 0xD6
						       0x00A6  // 0xD7
};

static const int ISO_6937_mapping3[0xFF - 0xDC + 1] = {0x215B, // 0xDC
						       0x215C, // 0xDD
						       0x215D, // 0xDE
						       0x215E, // 0xDF
						       0x2126, // 0xE0
						       0x00C6, // 0xE1
						       0x00D0, // 0xE2
						       0x00AA, // 0xE3
						       0x0126, // 0xE4
						       0x00E5, // <- is undefined in ISO 6937 !
						       0x0132, // 0xE6
						       0x013F, // 0xE7
						       0x0141, // 0xE8
						       0x00D8, // 0xE9
						       0x0152, // 0xEA
						       0x00BA, // 0xEB
						       0x00DE, // 0xEC
						       0x0166, // 0xED
						       0x014A, // 0xEE
						       0x0149, // 0xEF
						       0x0138, // 0xF0
						       0x00E6, // 0xF1
						       0x0111, // 0xF2
						       0x00F0, // 0xF3
						       0x0127, // 0xF4
						       0x0131, // 0xF5
						       0x0133, // 0xF6
						       0x0140, // 0xF7
						       0x0142, // 0xF8
						       0x00F8, // 0xF9
						       0x0153, // 0xFA
						       0x00DF, // 0xFB
						       0x00FE, // 0xFC
						       0x0167, // 0xFD
						       0x014B, // 0xFE
						       0x00AD  // 0xFF
};

unsigned int CDVBString::add_character(const unsigned char character, const unsigned char next_character)
{
	int character_unicode_value = character;
	unsigned int characters_parsed = 1;
	switch (encoding)
	{
	case ISO_8859_5:
		if ((character >= 0xA1) && (character != 0xAD))
		{
			switch (character)
			{
			case 0xF0:
				character_unicode_value = 0x2116;
				break;
			case 0xFD:
				character_unicode_value = 0x00A7;
				break;
			default:
				character_unicode_value += (0x0401 - 0xA1);
				break;
			}
		}
		break;

	case ISO_8859_9:
		switch (character)
		{
		case 0xD0:
			character_unicode_value = 0x011E;
			break;
		case 0xDD:
			character_unicode_value = 0x0130;
			break;
		case 0xDE:
			character_unicode_value = 0x015E;
			break;
		case 0xF0:
			character_unicode_value = 0x011F;
			break;
		case 0xFD:
			character_unicode_value = 0x0131;
			break;
		case 0xFE:
			character_unicode_value = 0x015F;
			break;
		}
		break;

	case ISO_6937:
		if ((character >= 0xA8) && (character <= 0xBA))
			character_unicode_value = ISO_6937_mapping[character - 0xA8];
		else if ((character >= 0xD0) && (character <= 0xD7))
			character_unicode_value = ISO_6937_mapping2[character - 0xD0];
		else if (character >= 0xDC)
			character_unicode_value = ISO_6937_mapping3[character - 0xDC];
		else
			if ((character >= 0xC0) && (character <= 0xCF))
			{
				if (next_character != 0x00)
				{
					characters_parsed = 2;

					switch (character)
					{
					case 0xC1:
					case 0xC2:
					case 0xC3:
						switch (next_character & (0xFF - 0x20))
						{
						case 0x41:
							character_unicode_value = 0x00C0;
							break;
						case 0x45:
							character_unicode_value = 0x00C8;
							break;
						case 0x49:
							character_unicode_value = 0x00CC;
							break;
						case 0x4f:
							character_unicode_value = 0x00D2;
							break;
						case 0x55:
							character_unicode_value = 0x00D9;
							break;
						default:
							character_unicode_value = next_character; // TODO: Implement the remaining conversion
							goto skip_adjustment;
						}
						character_unicode_value += (character - 0xC1) + (next_character & 0x20);
					skip_adjustment:
						break;
					default:
						character_unicode_value = next_character; // TODO: Implement the remaining conversion
						break;
					}
				}
			}
		break;

	default: // UNKNOWN
		break;
	}
	content += Unicode_Character_to_UTF8(character_unicode_value);
	return characters_parsed;
}
	
CDVBString::CDVBString(const char * the_content, const int size)
{
	int i;

	if (size > 0)
	{
		if ((((unsigned char)the_content[0]) >= 0x01) && (((unsigned char)the_content[0]) <= 0x05))
			encoding = (t_encoding)((unsigned char)the_content[0]);
		else if (((unsigned char)the_content[0]) >= 0x20)
			encoding = ISO_6937;
		else
			encoding = UNKNOWN;  // resp. we do not handle them
	}
	else
		encoding = UNKNOWN;

	for (i = 0; i < size; i++)                            // skip initial encoding information
		if (((unsigned char)the_content[i]) >= 0x20)
			break;

	if (size - i == 0)
		content = "";
	else
	{
		while(i < size)
		{
			// skip characters 0x00 - 0x1F & 0x80 - 0x9F
			if ((((unsigned char)the_content[i]) & 0x60) != 0)
				i += add_character((unsigned char)the_content[i], (unsigned char)the_content[i + 1]);
			else
				i++;
		}
	}
};

bool CDVBString::operator==(const CDVBString s)
{
	return (this->content == s.content);
};

bool CDVBString::operator!=(const CDVBString s)
{
	return !(operator==(s));
};

std::string CDVBString::getContent()
{
	return content;
};
