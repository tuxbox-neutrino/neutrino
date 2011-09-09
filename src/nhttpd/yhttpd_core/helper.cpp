//=============================================================================
// YHTTPD
// Helper
//=============================================================================

// c
#include <cstdio> 		// printf prototype.
#include <cstdlib> 		// calloc and free prototypes.
#include <cstring>		// str* and memset prototypes.
#include <cstdarg>
#include <sstream>
#include <iomanip>

// yhttpd
#include "yconfig.h"
#include "ytypes_globals.h"
#include "helper.h"
#include "ylogging.h"

//=============================================================================
// Integers
//=============================================================================
//-------------------------------------------------------------------------
// Check and set integer inside boundaries (min, max)
//-------------------------------------------------------------------------
int minmax(int value, int min, int max) {
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}
//=============================================================================
// Date & Time
//=============================================================================
//-------------------------------------------------------------------------
// Check and set Date/Time (tm*) inside boundaries
//-------------------------------------------------------------------------
void correctTime(struct tm *zt) {

	zt->tm_year = minmax(zt->tm_year, 0, 129);
	zt->tm_mon = minmax(zt->tm_mon, 0, 11);
	zt->tm_mday = minmax(zt->tm_mday, 1, 31); //-> eine etwas laxe pruefung, aber mktime biegt das wieder grade
	zt->tm_hour = minmax(zt->tm_hour, 0, 23);
	zt->tm_min = minmax(zt->tm_min, 0, 59);
	zt->tm_sec = minmax(zt->tm_sec, 0, 59);
}
//=============================================================================
// Strings
//=============================================================================
//-------------------------------------------------------------------------
// Integer to Hexadecimal-String
//-------------------------------------------------------------------------
std::string itoh(unsigned int conv) {
	return string_printf("0x%06x", conv);
}
//-------------------------------------------------------------------------
// Integer to String
//-------------------------------------------------------------------------
std::string itoa(unsigned int conv) {
	return string_printf("%u", conv);
}
//-------------------------------------------------------------------------
// convert timer_t to "<hour>:<minutes>" String
//-------------------------------------------------------------------------
std::string timeString(time_t time) {
	char tmp[7] = { '\0' };
	struct tm *tm = localtime(&time);
	if (strftime(tmp, 6, "%H:%M", tm))
		return std::string(tmp);
	else
		return std::string("??:??");
}
//-------------------------------------------------------------------------
// Printf and return formatet String. Buffer-save!
// max length up to bufferlen -> then snip
//-------------------------------------------------------------------------
#define bufferlen 4*1024
std::string string_printf(const char *fmt, ...) {
	char buffer[bufferlen];
	va_list arglist;
	va_start(arglist, fmt);
	vsnprintf(buffer, bufferlen, fmt, arglist);
	va_end(arglist);
	return std::string(buffer);
}
//-------------------------------------------------------------------------
// ySplitString: spit string "str" in two strings "left" and "right" at
//	one of the chars in "delimiter" returns true if delimiter found
//-------------------------------------------------------------------------
bool ySplitString(std::string str, std::string delimiter, std::string& left,
		std::string& right) {
	std::string::size_type pos;
	if ((pos = str.find_first_of(delimiter)) != std::string::npos) {
		left = str.substr(0, pos);
		right = str.substr(pos + 1, str.length() - (pos + 1));
	} else {
		left = str; //default if not found
		right = "";
	}
	return (pos != std::string::npos);
}
//-------------------------------------------------------------------------
// ySplitString: spit string "str" in two strings "left" and "right" at
//	one of the chars in "delimiter" returns true if delimiter found
//-------------------------------------------------------------------------
bool ySplitStringExact(std::string str, std::string delimiter,
		std::string& left, std::string& right) {
	std::string::size_type pos;
	if ((pos = str.find(delimiter)) != std::string::npos) {
		left = str.substr(0, pos);
		right = str.substr(pos + delimiter.length(), str.length() - (pos
				+ delimiter.length()));
	} else {
		left = str; //default if not found
		right = "";
	}
	return (pos != std::string::npos);
}
//-------------------------------------------------------------------------
// ySplitStringRight: spit string "str" in two strings "left" and "right" at
//	one of the chars in "delimiter" returns true if delimiter found
//-------------------------------------------------------------------------
bool ySplitStringLast(std::string str, std::string delimiter,
		std::string& left, std::string& right) {
	std::string::size_type pos;
	if ((pos = str.find_last_of(delimiter)) != std::string::npos) {
		left = str.substr(0, pos);
		right = str.substr(pos + 1, str.length() - (pos + 1));
	} else {
		left = str; //default if not found
		right = "";
	}
	return (pos != std::string::npos);
}
//-------------------------------------------------------------------------
// ySplitStringVector: spit string "str" and build vector of strings
//-------------------------------------------------------------------------
CStringArray ySplitStringVector(std::string str, std::string delimiter) {
	std::string left, right, rest;
	bool found;
	CStringArray split;
	rest = str;
	do {
		found = ySplitString(rest, delimiter, left, right);
		split.push_back(left);
		rest = right;
	} while (found);
	return split;
}
//-------------------------------------------------------------------------
// trim whitespaces
//-------------------------------------------------------------------------
std::string trim(std::string const& source, char const* delims) {
	std::string result(source);
	std::string::size_type index = result.find_last_not_of(delims);
	if (index != std::string::npos)
		result.erase(++index);

	index = result.find_first_not_of(delims);
	if (index != std::string::npos)
		result.erase(0, index);
	else
		result.erase();
	return result;
}
//-------------------------------------------------------------------------
// replace all occurrences find_what
//-------------------------------------------------------------------------
void replace(std::string &str, const std::string &find_what,
		const std::string &replace_with) {
	std::string::size_type pos = 0;
	while ((pos = str.find(find_what, pos)) != std::string::npos) {
		str.erase(pos, find_what.length());
		str.insert(pos, replace_with);
		pos += replace_with.length();
	}
}
//-------------------------------------------------------------------------
// equal-function for case insensitive compare
//-------------------------------------------------------------------------
bool nocase_compare(char c1, char c2) {
	return toupper(c1) == toupper(c2);
}
//-----------------------------------------------------------------------------
// Decode URLEncoded std::string
//-----------------------------------------------------------------------------
std::string decodeString(std::string encodedString) {
	const char *string = encodedString.c_str();
	unsigned int count = 0;
	char hex[3] = { '\0' };
	unsigned long iStr;
	std::string result = "";
	count = 0;

	while (count < encodedString.length()) /* use the null character as a loop terminator */
	{
		if (string[count] == '%' && count + 2 < encodedString.length()) {
			hex[0] = string[count + 1];
			hex[1] = string[count + 2];
			hex[2] = '\0';
			iStr = strtoul(hex, NULL, 16); /* convert to Hex char */
			result += (char) iStr;
			count += 3;
		} else if (string[count] == '+') {
			result += ' ';
			count++;
		} else {
			result += string[count];
			count++;
		}
	} /* end of while loop */
	return result;
}
//-----------------------------------------------------------------------------
// Encode URLEncoded std::string
//-----------------------------------------------------------------------------
std::string encodeString(std::string decodedString) {
	unsigned int len = sizeof(char) * decodedString.length() * 5 + 1;
	std::string result(len, '\0');
	char *newString = (char *) result.c_str();
	char *dstring = (char *) decodedString.c_str();
	char one_char;
	if (len == result.length()) // got memory needed
	{
		while ((one_char = *dstring++)) /* use the null character as a loop terminator */
		{
			if (isalnum(one_char))
				*newString++ = one_char;
			else
				newString += sprintf(newString, "&#%d;",
						(unsigned char) one_char);
		}

		*newString = '\0'; /* when done copying the string,need to terminate w/ null char */
		result.resize((unsigned int) (newString - result.c_str()), '\0');
		return result;
	} else {
		return "";
	}
}

//-----------------------------------------------------------------------------
// returns string in lower case
//-----------------------------------------------------------------------------
std::string string_tolower(std::string str) {
	for (unsigned int i = 0; i < str.length(); i++)
		str[i] = tolower(str[i]);
	return str;
}

//-----------------------------------------------------------------------------
// write string to a file
//-----------------------------------------------------------------------------
bool write_to_file(std::string filename, std::string content) {
	FILE *fd = NULL;
	if ((fd = fopen(filename.c_str(), "w")) != NULL) // open file
	{
		fwrite(content.c_str(), content.length(), 1, fd);
		fflush(fd); // flush and close file
		fclose(fd);
		return true;
	} else
		return false;
}

//-----------------------------------------------------------------------------
// JSON: create pair string "<_key>". "<_value>"
// Handle wrong quotes
//-----------------------------------------------------------------------------
std::string json_out_quote_convert(std::string _str) {
	replace(_str, "\"", "\'");
	return _str;
}
//-----------------------------------------------------------------------------
// JSON: create pair string "<_key>". "<_value>"
// Handle wrong quotes
//-----------------------------------------------------------------------------
std::string json_out_pair(std::string _key, std::string _value) {
	replace(_key, "\"", "");
	replace(_value, "\"", "\'");
	return "\"" + _key + "\": " + "\"" + _value + "\"";
}
//-----------------------------------------------------------------------------
// JSON: create success return string
//-----------------------------------------------------------------------------
std::string json_out_success(std::string _result) {
	return "{\"success\": \"true\", \"data\":{" + _result + "}}";
}
//-----------------------------------------------------------------------------
// JSON: create success return string
//-----------------------------------------------------------------------------
std::string json_out_error(std::string _error) {
	return "{\"success\": \"false\", \"error\":{\"text\": \"" + _error + "\"}}";
}
//-----------------------------------------------------------------------------
// JSON: convert string to JSON-String
//-----------------------------------------------------------------------------
std::string json_convert_string(std::string s) {
	std::stringstream ss;
	for (size_t i = 0; i < s.length(); ++i) {
		if (unsigned(s[i]) < '\x20' || s[i] == '\\' || s[i] == '"' || unsigned(s[i]) >= '\x80') {
			ss << "\\u" << std::setfill('0') << std::setw(4) << std::hex
					<< unsigned(s[i]);
		}
		else {
			ss << s[i];
		}
	}
	return ss.str();
}

