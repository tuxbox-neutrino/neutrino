//=============================================================================
// YHTTPD
// Logging & Debugging
//=============================================================================

// c
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

// yhttpd
#include <yconfig.h>
#include "ytypes_globals.h"
#include "ylogging.h"
#include "yconnection.h"
//=============================================================================
// Instance Handling - like Singelton Pattern
//=============================================================================
//-----------------------------------------------------------------------------
// Init as Singelton
//-----------------------------------------------------------------------------
CLogging *CLogging::instance = NULL;

//-----------------------------------------------------------------------------
// There is only one Instance
//-----------------------------------------------------------------------------
CLogging *CLogging::getInstance(void) {
	if (!instance)
		instance = new CLogging();
	return instance;
}

//-----------------------------------------------------------------------------
void CLogging::deleteInstance(void) {
	if (instance)
		delete instance;
	instance = NULL;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CLogging::CLogging(void) {
	Debug = false;
	LogToFile = false; //not implemented
	LogLevel = 0;
	Logfile = NULL;
	pthread_mutex_init(&Log_mutex, NULL);
}

//-----------------------------------------------------------------------------
CLogging::~CLogging(void) {
}
//=============================================================================

//-----------------------------------------------------------------------------
void CLogging::setDebug(bool _debug) {
	Debug = _debug;
}
//-----------------------------------------------------------------------------
bool CLogging::getDebug(void) {
	return Debug;
}
//=============================================================================
// Logging Calls
// use mutex controlled calls to output resources
// Normal Logging to Stdout, if "Log" is true then Log to file
//=============================================================================
#define bufferlen 1024*8
//-----------------------------------------------------------------------------
void CLogging::printf(const char *fmt, ...) {
	char buffer[bufferlen];

	va_list arglist;
	va_start(arglist, fmt);
	const int n = vsnprintf(buffer, bufferlen, fmt, arglist);
	va_end(arglist);

	pthread_mutex_lock(&Log_mutex);
	buffer[bufferlen-1]='\0';
	// a line cut short at the end of the buffer still ends, so that the next
	// one starts a line of its own
	if (n >= bufferlen)
		buffer[bufferlen-2]='\n';
	::printf("%s",buffer);
	fflush(stdout);

	if (LogToFile) {
		; //FIXME Logging to File
	}
	pthread_mutex_unlock(&Log_mutex);
}


//=============================================================================
// What of a request may stand in a log
//=============================================================================
namespace ylog
{

std::string text(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++) {
		const unsigned char c = (unsigned char) s[i];
		switch (c) {
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		case '\\': out += "\\\\"; break;
		case '"': out += "\\\""; break;
		default:
			if (c < 0x20 || c == 0x7f) {
				char esc[5];
				snprintf(esc, sizeof(esc), "\\x%02x", c);
				out += esc;
			} else
				out += (char) c;
		}
	}
	return out;
}

namespace
{

// name without the blanks a header line may carry around it
std::string bare(const std::string &name)
{
	const size_t from = name.find_first_not_of(" \t");
	if (from == std::string::npos)
		return std::string();
	const size_t to = name.find_last_not_of(" \t");
	return name.substr(from, to - from + 1);
}

bool endsWith(const std::string &s, const char *tail)
{
	const size_t n = strlen(tail);
	return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

const char kRedacted[] = "[redacted]";

// a name ending in _pass_<n>, the password a setting keeps for the n-th entry
// of a list (timer_remotebox_pass_<n>); ac3_pass and dts_pass are switches
bool numberedPass(const std::string &n)
{
	const size_t at = n.rfind("_pass_");
	if (at == std::string::npos || at + 6 == n.size())
		return false;
	return n.find_first_not_of("0123456789", at + 6) == std::string::npos;
}

// what of a header's or a parameter's value may stand in a log, where its name
// does not make it a secret: an address as url() writes it, which leaves the
// query's values and a user and password in front of the host out
std::string plainValue(const std::string &value)
{
	return value.find("://") != std::string::npos ? url(value) : text(value);
}

} // namespace

bool secretHeader(const std::string &name)
{
	static const char *const secret[] = {
		"Authorization", "Proxy-Authorization", "Cookie", "Set-Cookie", "X-CSRF-Token"
	};
	const std::string n = bare(name);
	for (size_t i = 0; i < sizeof(secret) / sizeof(secret[0]); i++)
		if (strcasecmp(n.c_str(), secret[i]) == 0)
			return true;
	return false;
}

bool secretParam(const std::string &name)
{
	std::string n = bare(name);
	for (size_t i = 0; i < n.size(); i++)
		if (n[i] >= 'A' && n[i] <= 'Z')
			n[i] = (char) (n[i] - 'A' + 'a');
	static const char *const inside[] = {
		"password", "passwd", "pincode", "token", "ticket", "secret"
	};
	for (size_t i = 0; i < sizeof(inside) / sizeof(inside[0]); i++)
		if (n.find(inside[i]) != std::string::npos)
			return true;
	/* And the names the pages and the routes send a credential under that
	   none of the above catches: the current password a change of the
	   account is asked with, and the short names a login form may use. */
	static const char *const whole[] = { "current", "pass", "pw", "pwd", "pin" };
	for (size_t i = 0; i < sizeof(whole) / sizeof(whole[0]); i++)
		if (n == whole[i])
			return true;
	return endsWith(n, "_key") || endsWith(n, "_dev_id") || numberedPass(n);
}

std::string headerValue(const std::string &name, const std::string &value)
{
	return secretHeader(name) ? std::string(kRedacted) : plainValue(value);
}

std::string paramValue(const std::string &name, const std::string &value)
{
	return secretParam(name) ? std::string(kRedacted) : plainValue(value);
}

std::string withheld(const std::string &s)
{
	char said[32];
	snprintf(said, sizeof(said), "%lu bytes", (unsigned long) s.size());
	return said;
}

std::string url(const std::string &target)
{
	const size_t mark = target.find('?');
	std::string head = target.substr(0, mark);
	// an absolute address: a user and password in front of the host go
	const size_t scheme = head.find("://");
	if (scheme != std::string::npos) {
		const size_t host = scheme + 3;
		const size_t end = head.find_first_of("/#", host);
		const size_t at = head.rfind('@', end == std::string::npos ? std::string::npos : end);
		if (at != std::string::npos && at >= host)
			head.replace(host, at - host, kRedacted);
	}
	std::string out = text(head);
	if (mark == std::string::npos)
		return out;
	out += '?';
	size_t at = mark + 1;
	while (true) {
		const size_t amp = target.find('&', at);
		const size_t end = (amp == std::string::npos) ? target.size() : amp;
		const size_t eq = target.find('=', at);
		if (eq != std::string::npos && eq < end)
			out += text(target.substr(at, eq - at)) + "=";
		if (amp == std::string::npos)
			break;
		out += '&';
		at = amp + 1;
	}
	return out;
}

std::string startLine(const std::string &line)
{
	const size_t first = line.find(' ');
	if (first == std::string::npos)
		return url(line);
	const size_t last = line.rfind(' ');
	if (last == first)
		return text(line.substr(0, first)) + " " + url(line.substr(first + 1));
	return text(line.substr(0, first)) + " " + url(line.substr(first + 1, last - first - 1))
		+ " " + text(line.substr(last + 1));
}

} // namespace ylog
