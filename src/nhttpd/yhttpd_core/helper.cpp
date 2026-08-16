//=============================================================================
// YHTTPD
// Helper
//=============================================================================

// c
#include <cstdio> 		// printf prototype.
#include <cstdlib> 		// calloc and free prototypes.
#include <cstring>		// str* and memset prototypes.
#include <cstdarg>
#include <cerrno>
#include <sstream>
#include <iomanip>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/wait.h>

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
bool write_to_file(std::string filename, std::string content, bool append) {
	FILE *fd = NULL;
	if ((fd = fopen(filename.c_str(), append ? "a" : "w")) != NULL) // open file
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

// Start file with the prepared argv in dir and collect its stdout.
// argv and shargv are built by the caller so that the child only makes
// async-signal-safe calls between fork() and execv() (getdtablesize() is a
// getrlimit() wrapper and safe in practice).
// Returns false when fork()/pipe() failed, i.e. no child ran at all. A child
// that could not exec reports itself through an empty result, exactly as the
// previous popen() based version did.
static bool yRunNoShell(const std::string &file, const std::string &dir,
		char *const argv[], char *const shargv[], std::string &out) {
	int fd[2];
	// O_CLOEXEC: without it a fork() in any other thread inherits our write
	// end and keeps it open, so the read loop below would not see EOF until
	// that unrelated child exits. popen() gave the same guarantee for its
	// own pipes, so plain pipe() here would be a regression.
	if (pipe2(fd, O_CLOEXEC) != 0)
		return false;

	pid_t pid = fork();
	if (pid < 0) {
		int err = errno; // keep it: close() may set errno on success
		close(fd[0]);
		close(fd[1]);
		errno = err;
		return false;
	}

	if (pid == 0) { // child
		close(fd[0]); // the child only writes
		if (fd[1] == STDOUT_FILENO) {
			// already in place, but must survive execv()
			int flags = fcntl(fd[1], F_GETFD);
			if (flags < 0 || fcntl(fd[1], F_SETFD, flags & ~FD_CLOEXEC) < 0)
				_exit(127);
		} else {
			if (dup2(fd[1], STDOUT_FILENO) < 0) // dup2 clears FD_CLOEXEC
				_exit(127);
			// close_range() below starts at 3 and would leave a write end
			// behind that happened to land on fd 2
			close(fd[1]);
		}
		// Do not leak the listening socket, open client connections or
		// driver handles into the script; ysocket.cpp creates them without
		// SOCK_CLOEXEC, so O_CLOEXEC on our own pipe does not cover them.
		// close_range() does this in one call - the fallback loop costs a
		// syscall per possible descriptor, which is tens of milliseconds
		// per script once RLIMIT_NOFILE is large (524288 on a PC build).
#if defined(SYS_close_range)
		if (syscall(SYS_close_range, 3U, ~0U, 0U) != 0)
#endif
		{
			int maxfd = getdtablesize();
			for (int i = 3; i < maxfd; i++)
				close(i);
		}
		if (chdir(dir.c_str()) != 0)
			_exit(127);
		execv(file.c_str(), argv);
		// popen() handed everything to /bin/sh, so a script without a
		// shebang used to run. Keep that working, but through argv rather
		// than a shell command line, so nothing is re-interpreted.
		if (errno == ENOEXEC)
			execv("/bin/sh", shargv);
		_exit(127);
	}

	close(fd[1]);
	const size_t readblocklen = 1024; // chunk size, not an output limit
	char buf[readblocklen];
	for (;;) {
		ssize_t n = read(fd[0], buf, readblocklen);
		if (n > 0)
			out.append(buf, (size_t) n);
		else if (n == 0 || errno != EINTR) // EINTR: keep reading, do not truncate
			break;
	}
	close(fd[0]);

	while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
		;
	return true;
}

std::string yExecuteScript(std::string cmd) {
	std::string script, para, result;
	bool found = false;
	bool launch_failed = false;

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
	script += ".sh"; //add script extension

	// The script name arrives from the same query string as the parameters.
	// Keep it a bare name so it cannot walk out of the plugin directories.
	if (script.find('/') != std::string::npos) {
		printf("%s: refused script name with a path separator: %s\n",
			__func__, script.c_str());
		return "error";
	}

	// Build the argument vector here instead of handing the whole command
	// line to a shell. The parameters come straight from the HTTP query
	// string; going through popen() meant /bin/sh split the words but also
	// interpreted metacharacters in them. Splitting on whitespace only means
	// ';', '|', '$(...)' and quotes all reach the script as plain text, so an
	// apostrophe in a file name survives instead of being eaten as a quote.
	// Callers encode their parameters, so a space still separates arguments.
	std::vector<std::string> args;
	size_t a = 0;
	while (a < para.length()) {
		a = para.find_first_not_of(" \t", a);
		if (a == std::string::npos)
			break;
		size_t e = para.find_first_of(" \t", a);
		if (e == std::string::npos)
			e = para.length();
		args.push_back(para.substr(a, e - a));
		a = e;
	}

	for (unsigned int i = 0; i < CControlAPI::PLUGIN_DIR_COUNT && !found; i++) {
		fullfilename = CControlAPI::PLUGIN_DIRS[i] + "/" + script;
		// X_OK instead of readability: a script without the execute bit was
		// counted as found and then returned silently empty output
		if (access(fullfilename.c_str(), X_OK) != 0)
			continue;

		// build argv before fork() so the child allocates nothing
		std::vector<char *> argv;
		argv.reserve(args.size() + 2);
		argv.push_back(const_cast<char *>(fullfilename.c_str()));
		for (size_t k = 0; k < args.size(); k++)
			argv.push_back(const_cast<char *>(args[k].c_str()));
		argv.push_back(NULL);

		// same list for the ENOEXEC fallback, prefixed with the shell
		char shname[] = "sh";
		std::vector<char *> shargv;
		shargv.reserve(argv.size() + 1);
		shargv.push_back(shname);
		shargv.insert(shargv.end(), argv.begin(), argv.end());

		// the child chdir()s itself; the old code changed the working
		// directory of the whole threaded process for the call's duration
		// A failed fork()/pipe() means resource exhaustion, not "wrong
		// directory": stop instead of running a same-named script from a
		// later one.
		if (!yRunNoShell(fullfilename, CControlAPI::PLUGIN_DIRS[i], &argv[0], &shargv[0], result)) {
			// say what really happened; the script was there, we just
			// could not start it
			// errno number rather than strerror(): nhttpd serves requests in
			// threads, and strerror() hands out a shared static buffer
			printf("%s: cannot start %s: errno %d\n", __func__,
				fullfilename.c_str(), errno);
			launch_failed = true;
			break;
		}
		found = true;
	}

	if (!found) {
		// only a real lookup miss gets the search-path dump - printing it
		// after a resource failure would send an operator hunting for a
		// file that is not missing at all
		if (!launch_failed) {
			printf("%s: script %s not found in:\n", __func__, script.c_str());
			for (unsigned int i = 0; i < CControlAPI::PLUGIN_DIR_COUNT; i++) {
				printf("\t%s\n", CControlAPI::PLUGIN_DIRS[i].c_str());
			}
		}
		result = "error";
	}
	return result;
}
