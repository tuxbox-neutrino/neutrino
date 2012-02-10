//
// $Id: SIutils.hpp,v 1.5 2006/05/19 21:28:08 houdini Exp $
//
// utility functions for the SI-classes (dbox-II-project)
//
//    Homepage: http://dbox2.elxsi.de
//
//    Copyright (C) 2001 fnbrd (fnbrd@gmx.de)
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#ifndef SIUTILS_HPP
#define SIUTILS_HPP

#include <stdint.h>
#include <string>

#define ENABLE_FREESATEPG //FIXME

time_t changeUTCtoCtime(const unsigned char *buffer, int local_time=1);
time_t parseDVBtime(uint16_t mjd, uint32_t bcd);

// returns the descriptor type as readable text
const char *decode_descr (unsigned char tag_value);

int saveStringToXMLfile(FILE *out, const char *string, int withControlCodes=0);

// Entfernt die ControlCodes aus dem String (-> String wird evtl. kuerzer)
void removeControlCodes(char *string);

#ifdef ENABLE_FREESATEPG
std::string freesatHuffmanDecode(std::string input);
#endif

#endif // SIUTILS_HPP
