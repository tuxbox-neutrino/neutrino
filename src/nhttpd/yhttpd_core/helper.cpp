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

#include <unistd.h>

// yhttpd
#include <yconfig.h>
#include <tuxboxapi/controlapi.h>
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
	zt->tm_isdst = -1;
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
std::string string_printf(const char *fmt, ...) {
	va_list arglist;
	const int bufferlen = 4*1024;
	char buffer[bufferlen] = {0};
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
	replace(left,  "\r\n", "");
	replace(left,  "\n",   "");
	replace(right, "\r\n", "");
	replace(right, "\n",   "");
	left = trim(left);
	right = trim(right);
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
// HTMLEncode std::string
//-----------------------------------------------------------------------------
std::string encodeString(const std::string &decodedString)
{
	std::string result="";
	char buf[10]= {0};

	for (unsigned int i=0; i<decodedString.length(); i++)
	{
		const char one_char = decodedString[i];
		if (isalnum(one_char)) {
			result += one_char;
		} else {
			snprintf(buf,sizeof(buf), "&#%d;",(unsigned char) one_char);
			result +=buf;
		}
	}
	result+='\0';
	result.reserve();
	return result;
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

std::string json_convert_string(std::string value)
{
	std::string result;
	for (size_t i = 0; i < value.length(); i++)
	{
		unsigned char c = unsigned(value[i]);
		switch(c)
		{
		case '\"':
			result += "\\\"";
			break;
		case '\\':
			result += "\\\\";
			break;
		case '\b':
			result += "\\b";
			break;
		case '\f':
			result += "\\f";
			break;
		case '\n':
			result += "\\n";
			break;
		case '\r':
			result += "\\r";
			break;
		case '\t':
			result += "\\t";
			break;
		default:
			if ( isControlCharacter( c ) )
			{
				std::ostringstream oss;
				oss << "\\u" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << static_cast<int>(c);
				result += oss.str();
			}
			else
			{
				result += c;
			}
			break;
		}
	}
	return result;
}

#if 0
std::string json_convert_string(std::string s) {
	std::stringstream ss;
	for (size_t i = 0; i < s.length(); ) {
		unsigned char ch = unsigned(s[i]);
		if(ch == 0x0d){
			ss << "\\u000d";
			i++;
			continue;
		}
		if(ch == 0x0a){
			ss << "\\u000a";
			i++;
			continue;
		}

		if(ch < '\x20' || ch == '\\' || ch == '"' || ch >= '\x80') {
			unsigned long unicode = 0;
			size_t todo = 0;
			if (ch <= 0xBF) {
			}
			else if (ch <= 0xDF) {
				unicode = ch & 0x1F;
				todo = 1;
			}
			else if (ch <= 0xEF) {
				unicode = ch & 0x0F;
				todo = 2;
			}
			else if (ch <= 0xF7) {
				unicode = ch & 0x07;
				todo = 3;
			}
			for (size_t j = 0; j < todo; ++j){
				++i;
				unicode <<= 6;
				unicode += unsigned(s[i]) & 0x3F;
			}
			if (unicode <= 0xFFFF)
			{
				ss << "\\u" << std::setfill('0') << std::setw(4) << std::hex << unicode;
			}else
			{
				unicode -= 0x10000;
				ss << "\\u" << std::setfill('0') << std::setw(4) << std::hex << ((unicode >> 10) + 0xD800);
				ss << "\\u" << std::setfill('0') << std::setw(4) << std::hex << ((unicode & 0x3FF) + 0xDC00);
			}
		}
		else {
			ss << s[i];
		}
		++i;
	}
	return ss.str();
}
#endif // 0

std::string yExecuteScript(std::string cmd) {
	std::string script, para, result;
	bool found = false;

	//aprintf("%s: %s\n", __func__, cmd.c_str());

	// split script and parameters
	int pos;
	if ((pos = cmd.find_first_of(" ")) > 0) {
		script = cmd.substr(0, pos);
		para = cmd.substr(pos + 1, cmd.length() - (pos + 1)); // snip
	} else
		script = cmd;
	// get file
	std::string fullfilename;
	script += ".sh"; //add script extention

	char cwd[255];
	getcwd(cwd, 254);
	for (unsigned int i = 0; i < CControlAPI::PLUGIN_DIR_COUNT && !found; i++) {
		fullfilename = CControlAPI::PLUGIN_DIRS[i] + "/" + script;
		FILE *test = fopen(fullfilename.c_str(), "r"); // use fopen: popen does not work
		if (test != NULL) {
			fclose(test);
			chdir(CControlAPI::PLUGIN_DIRS[i].c_str());
			FILE *f = popen((fullfilename + " " + para).c_str(), "r"); //execute
			if (f != NULL) {
				found = true;

				char output[1024];
				while (fgets(output, 1024, f)) // get script output
					result += output;
				pclose(f);
			}
		}
	}
	chdir(cwd);

	if (!found) {
		printf("%s: script %s not found in:\n", __func__, script.c_str());
		for (unsigned int i = 0; i < CControlAPI::PLUGIN_DIR_COUNT; i++) {
			printf("\t%s\n", CControlAPI::PLUGIN_DIRS[i].c_str());
		}
		result = "error";
	}
	return result;
}
