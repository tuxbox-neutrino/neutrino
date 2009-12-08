//=============================================================================
// YHTTPD
// Helper
//=============================================================================

#ifndef __yhttpd_helper_h__
#define __yhttpd_helper_h__
// c
#include <ctime>
#include <sys/timeb.h>
// c++
#include <string>
#include <vector>
#include "ytypes_globals.h"

//-----------------------------------------------------------------------------
int minmax(int value,int min, int max);
void correctTime(struct tm *zt);

//-----------------------------------------------------------------------------
// String Converter
//-----------------------------------------------------------------------------
std::string itoa(unsigned int conv);
std::string itoh(unsigned int conv);
std::string decodeString(std::string encodedString);
std::string encodeString(std::string decodedString);
std::string string_tolower(std::string str);

//-----------------------------------------------------------------------------
// String Helpers
//-----------------------------------------------------------------------------
std::string trim(std::string const& source, char const* delims = " \t\r\n");
std::string string_printf(const char *fmt, ...);
bool ySplitString(std::string str, std::string delimiter, std::string& left, std::string& right);
bool ySplitStringExact(std::string str, std::string delimiter, std::string& left, std::string& right);
bool ySplitStringLast(std::string str, std::string delimiter, std::string& left, std::string& right);
CStringArray ySplitStringVector(std::string str, std::string delimiter);
bool nocase_compare (char c1, char c2);
std::string timeString(time_t time);
bool write_to_file(std::string filename, std::string content);

#endif /* __yhttpd_helper_h__ */
