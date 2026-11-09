/*
 * test_weblog.cpp - where the server writes its log, and what of a request
 * a log line holds
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The log goes into one directory, the one the default log file lies in,
 * and nowhere else: not through a link, not into a file with a second name
 * or one that is no regular file, and never into a file that holds who may
 * reach the box, whatever name it is reached by. A record is one line,
 * whatever the request carried; the values of its query are left out and a
 * credential stands as [redacted], in both formats of the access log and in
 * every debug line the parser writes.
 */

#include "support/catch.hpp"

#include "yconnection.h"
#include "yhook.h"
#include "ylogging.h"
#include "yprotect.h"
#include "yrequest.h"
#include "ysocket.h"
#include "ywebserver.h"
#include "yhttpd_mods/mod_weblog.h"

#include <configfile.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <ftw.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

int removeEntry(const char *path, const struct stat *, int, struct FTW *)
{
	return remove(path);
}

// A directory for the log, one beside it, and a place above both.
struct Dirs
{
	std::string root, log, other;

	Dirs()
	{
		char dir[] = "/tmp/test_weblog_XXXXXX";
		REQUIRE(mkdtemp(dir) != NULL);
		root = dir;
		log = root + "/log";
		other = root + "/other";
		REQUIRE(mkdir(log.c_str(), 0700) == 0);
		REQUIRE(mkdir(other.c_str(), 0700) == 0);
	}
	~Dirs()
	{
		yprotect::setFiles(std::vector<std::string>());
		nftw(root.c_str(), removeEntry, 16, FTW_DEPTH | FTW_PHYS);
	}
};

bool exists(const std::string &path)
{
	struct stat st;
	return lstat(path.c_str(), &st) == 0;
}

void writeFile(const std::string &path, const std::string &text)
{
	const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	REQUIRE(fd >= 0);
	REQUIRE(write(fd, text.data(), text.size()) == (ssize_t) text.size());
	close(fd);
}

std::string readFile(const std::string &path)
{
	std::string out;
	const int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0)
		return out;
	char buf[512];
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		out.append(buf, (size_t) n);
	close(fd);
	return out;
}

// openIn for path in dir: true where a log would be written there
bool opens(const std::string &path, const std::string &dir)
{
	std::string why;
	const int fd = CmWebLog::openIn(path, dir, why);
	if (fd < 0)
		return false;
	close(fd);
	return true;
}

// why openIn turns path down in dir, empty where it does not
std::string refused(const std::string &path, const std::string &dir)
{
	std::string why;
	const int fd = CmWebLog::openIn(path, dir, why);
	if (fd >= 0) {
		close(fd);
		return std::string();
	}
	return why.empty() ? std::string("(no reason given)") : why;
}

size_t count(const std::string &s, char c)
{
	size_t n = 0;
	for (size_t i = 0; i < s.size(); i++)
		if (s[i] == c)
			n++;
	return n;
}

// Every line of text that holds marker begins as a log line of the server
// does, so no line break inside a request has put it at a start of its own.
bool markerNeverStartsALine(const std::string &text, const std::string &marker)
{
	size_t start = 0;
	while (start < text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();
		const std::string line = text.substr(start, end - start);
		if (line.find(marker) != std::string::npos && line.compare(0, 7, "[yhttpd") != 0)
			return false;
		start = end + 1;
	}
	return true;
}

} // namespace

TEST_CASE("the log is opened directly in its own directory and nowhere else", "[nhttpd]")
{
	Dirs d;
	REQUIRE(mkdir((d.log + "/sub").c_str(), 0700) == 0);

	// an ordinary file there is the log, added to and never cut short
	std::string why;
	int fd = CmWebLog::openIn(d.log + "/access.log", d.log, why);
	REQUIRE(fd >= 0);
	CHECK(write(fd, "one\n", 4) == 4);
	close(fd);
	fd = CmWebLog::openIn(d.log + "/access.log", d.log, why);
	REQUIRE(fd >= 0);
	CHECK(write(fd, "two\n", 4) == 4);
	close(fd);
	CHECK(readFile(d.log + "/access.log") == "one\ntwo\n");
	// and one it makes is its owner's alone
	struct stat st;
	REQUIRE(stat((d.log + "/access.log").c_str(), &st) == 0);
	CHECK((st.st_mode & 077) == 0);

	// the directory is what is compared, not the way it is spelt
	REQUIRE(symlink(d.log.c_str(), (d.root + "/alias").c_str()) == 0);
	CHECK(opens(d.root + "/alias/spelt.log", d.log));
	CHECK(exists(d.log + "/spelt.log"));

	// anywhere else nothing is opened, and nothing is made either
	const std::string elsewhere[] = {
		d.other + "/c.log", d.log + "/sub/c.log", d.log + "/../c.log",
		"c.log", d.log + "/", d.log + "/.", d.log + "/..",
	};
	for (size_t i = 0; i < sizeof(elsewhere) / sizeof(elsewhere[0]); i++) {
		INFO(elsewhere[i]);
		CHECK_FALSE(refused(elsewhere[i], d.log).empty());
	}
	CHECK_FALSE(exists(d.other + "/c.log"));
	CHECK_FALSE(exists(d.log + "/sub/c.log"));
	CHECK_FALSE(exists(d.log + "/c.log"));
	CHECK_FALSE(exists(d.root + "/c.log"));
}

TEST_CASE("a link, a second name or a file of another kind is no log", "[nhttpd]")
{
	Dirs d;
	writeFile(d.log + "/real.log", "kept\n");

	// a link is not followed, to a file beside it or to one not there yet
	REQUIRE(symlink((d.log + "/real.log").c_str(), (d.log + "/link.log").c_str()) == 0);
	CHECK_FALSE(refused(d.log + "/link.log", d.log).empty());
	CHECK(readFile(d.log + "/real.log") == "kept\n");
	REQUIRE(symlink((d.other + "/made.log").c_str(), (d.log + "/dangling.log").c_str()) == 0);
	CHECK_FALSE(refused(d.log + "/dangling.log", d.log).empty());
	CHECK_FALSE(exists(d.other + "/made.log"));

	// a file with a second name is told apart by what was opened: under
	// either of its names
	CHECK(opens(d.log + "/real.log", d.log));
	REQUIRE(link((d.log + "/real.log").c_str(), (d.log + "/second.log").c_str()) == 0);
	CHECK_FALSE(refused(d.log + "/second.log", d.log).empty());
	CHECK_FALSE(refused(d.log + "/real.log", d.log).empty());

	// a FIFO is turned down without waiting for a reader, and with one, by
	// what was opened; a directory too
	REQUIRE(mkfifo((d.log + "/fifo.log").c_str(), 0600) == 0);
	CHECK_FALSE(refused(d.log + "/fifo.log", d.log).empty());
	const int reader = open((d.log + "/fifo.log").c_str(), O_RDONLY | O_NONBLOCK);
	REQUIRE(reader >= 0);
	CHECK_FALSE(refused(d.log + "/fifo.log", d.log).empty());
	close(reader);
	REQUIRE(mkdir((d.log + "/dir.log").c_str(), 0700) == 0);
	CHECK_FALSE(refused(d.log + "/dir.log", d.log).empty());
}

TEST_CASE("a file that holds who may reach the box is never a log, by whatever name", "[nhttpd]")
{
	Dirs d;
	writeFile(d.log + "/access.conf", "password_hash=x\n");
	writeFile(d.log + "/store", "token\n");
	REQUIRE(symlink((d.log + "/store").c_str(), (d.log + "/store-link").c_str()) == 0);
	REQUIRE(symlink(d.log.c_str(), (d.root + "/alias").c_str()) == 0);
	std::vector<std::string> files;
	files.push_back(d.log + "/access.conf");
	files.push_back(d.log + "/access.conf.new");
	files.push_back(d.log + "/store-link");
	yprotect::setFiles(files);

	// by its own name, also one not there yet, which is not made
	CHECK_FALSE(refused(d.log + "/access.conf", d.log).empty());
	CHECK(readFile(d.log + "/access.conf") == "password_hash=x\n");
	CHECK_FALSE(refused(d.log + "/access.conf.new", d.log).empty());
	CHECK_FALSE(exists(d.log + "/access.conf.new"));
	// through another spelling of its directory
	CHECK_FALSE(refused(d.root + "/alias/access.conf", d.log).empty());
	// a store named by a link is the file the link leads to
	CHECK_FALSE(refused(d.log + "/store", d.log).empty());
	CHECK(readFile(d.log + "/store") == "token\n");

	// an ordinary file beside them is still a log
	CHECK(opens(d.log + "/ordinary.log", d.log));

	// and the store is turned down for what it is: once nothing is named,
	// the same file opens
	yprotect::setFiles(std::vector<std::string>());
	CHECK(opens(d.log + "/store", d.log));
}

TEST_CASE("a log named outside its directory is not written, and the server says why", "[nhttpd]")
{
	// the directory is the one the default log file lies in
	const std::string dflt = LOG_FILE;
	CHECK(dflt.substr(0, dflt.rfind('/')) == CmWebLog::logDirectory());

	Dirs d;
	CyhookHandler hh;
	hh.UrlData["clientaddr"] = "192.0.2.1";
	hh.UrlData["startline"] = "GET /index.html HTTP/1.1";
	hh.UrlData["target"] = "/index.html";
	hh.Method = M_GET;
	hh.httpStatus = HTTP_OK;

	CmWebLog weblog;
	CStringList list;
	CConfigFile config(',');
	config.setString("mod_weblog.log_format", "CLF");
	config.setString("mod_weblog.logfile", d.other + "/access.log");
	weblog.Hook_ReadConfig(&config, list);
	weblog.Hook_EndConnection(&hh);
	CHECK_FALSE(exists(d.other + "/access.log"));
	CHECK_FALSE(CmWebLog::refusal().empty());

	// in the directory itself it is written, a record to a line
	char name[] = "/tmp/test_weblog_log_XXXXXX";
	const int made = mkstemp(name);
	REQUIRE(made >= 0);
	close(made);
	REQUIRE(CmWebLog::logDirectory() == "/tmp");
	config.setString("mod_weblog.logfile", name);
	weblog.Hook_ReadConfig(&config, list);
	CHECK(CmWebLog::refusal().empty());
	weblog.Hook_EndConnection(&hh);
	weblog.Hook_EndConnection(&hh);
	const std::string written = readFile(name);
	CHECK(count(written, '\n') == 2);
	CHECK(written.find("\"GET /index.html HTTP/1.1\" 200") != std::string::npos);

	config.setString("mod_weblog.log_format", "");
	config.setString("mod_weblog.logfile", LOG_FILE);
	weblog.Hook_ReadConfig(&config, list);
	unlink(name);
}

TEST_CASE("a log named anew is written from the next record on, the old one left alone", "[nhttpd]")
{
	char first[] = "/tmp/test_weblog_log_XXXXXX";
	char second[] = "/tmp/test_weblog_log_XXXXXX";
	int made = mkstemp(first);
	REQUIRE(made >= 0);
	close(made);
	made = mkstemp(second);
	REQUIRE(made >= 0);
	close(made);
	CyhookHandler hh;
	hh.UrlData["clientaddr"] = "192.0.2.1";
	hh.UrlData["startline"] = "GET / HTTP/1.1";
	hh.Method = M_GET;
	hh.httpStatus = HTTP_OK;

	CmWebLog weblog;
	CStringList list;
	CConfigFile config(',');
	config.setString("mod_weblog.log_format", "CLF");
	config.setString("mod_weblog.logfile", first);
	weblog.Hook_ReadConfig(&config, list);
	weblog.Hook_EndConnection(&hh);
	config.setString("mod_weblog.logfile", second);
	weblog.Hook_ReadConfig(&config, list);
	weblog.Hook_EndConnection(&hh);
	CHECK(count(readFile(first), '\n') == 1);
	CHECK(count(readFile(second), '\n') == 1);

	config.setString("mod_weblog.log_format", "");
	config.setString("mod_weblog.logfile", LOG_FILE);
	weblog.Hook_ReadConfig(&config, list);
	unlink(first);
	unlink(second);
}

TEST_CASE("a log that cannot be opened for a moment is tried again with the next record", "[nhttpd]")
{
	char name[] = "/tmp/test_weblog_log_XXXXXX";
	const int made = mkstemp(name);
	REQUIRE(made >= 0);
	close(made);
	CyhookHandler hh;
	hh.UrlData["clientaddr"] = "192.0.2.1";
	hh.UrlData["startline"] = "GET / HTTP/1.1";
	hh.Method = M_GET;
	hh.httpStatus = HTTP_OK;

	CmWebLog weblog;
	CStringList list;
	CConfigFile config(',');
	config.setString("mod_weblog.log_format", "CLF");
	config.setString("mod_weblog.logfile", name);
	weblog.Hook_ReadConfig(&config, list);

	// no descriptor left for a moment: the record is lost, the log stays on
	struct rlimit was;
	REQUIRE(getrlimit(RLIMIT_NOFILE, &was) == 0);
	const int lowest = open("/dev/null", O_RDONLY);
	REQUIRE(lowest >= 0);
	close(lowest);
	struct rlimit tight = was;
	tight.rlim_cur = (rlim_t) lowest;
	REQUIRE(setrlimit(RLIMIT_NOFILE, &tight) == 0);
	weblog.Hook_EndConnection(&hh);
	REQUIRE(setrlimit(RLIMIT_NOFILE, &was) == 0);
	CHECK(readFile(name).empty());
	CHECK(CmWebLog::refusal().empty());

	// and the next record, with descriptors again, is written
	weblog.Hook_EndConnection(&hh);
	CHECK(count(readFile(name), '\n') == 1);

	config.setString("mod_weblog.log_format", "");
	config.setString("mod_weblog.logfile", LOG_FILE);
	weblog.Hook_ReadConfig(&config, list);
	unlink(name);
}

TEST_CASE("the helpers escape, leave a query's values out and redact credentials", "[nhttpd]")
{
	CHECK(ylog::text(std::string("a\nb\r\tc\x01\x7f\"\\ d\xc3\xa4", 14)) == "a\\nb\\r\\tc\\x01\\x7f\\\"\\\\ d\xc3\xa4");
	CHECK(ylog::text(std::string("a\0b", 3)) == "a\\x00b");

	CHECK(ylog::url("/control/config?a=1&b&c=3=4&&d=") == "/control/config?a=&&c=&&d=");
	CHECK(ylog::url("/p/a%0Ab") == "/p/a%0Ab");
	CHECK(ylog::url("/p?") == "/p?");
	CHECK(ylog::startLine("GET /x?password=S3NT1NEL HTTP/1.1") == "GET /x?password= HTTP/1.1");
	CHECK(ylog::startLine("GET /a b?k=S3NT1NEL HTTP/1.0") == "GET /a b?k= HTTP/1.0");
	CHECK(ylog::startLine("GET /x?k=S3NT1NEL") == "GET /x?k=");
	CHECK(ylog::startLine("/x?k=S3NT1NEL") == "/x?k=");

	const char *const secretHeaders[] = {
		"Authorization", "authorization", " Cookie ", "Set-Cookie", "Proxy-Authorization", "x-csrf-token"
	};
	for (size_t i = 0; i < sizeof(secretHeaders) / sizeof(secretHeaders[0]); i++) {
		INFO(secretHeaders[i]);
		CHECK(ylog::headerValue(secretHeaders[i], "Basic S3NT1NEL") == "[redacted]");
	}
	CHECK(ylog::headerValue("Host", "box\n") == "box\\n");

	// the names that carry secrets in this tree: the account over
	// /control/config, the API token in a query, the settings' passwords,
	// PINs, API keys and developer IDs
	const char *const secretParams[] = {
		"password", "mod_auth_dot_password", "mod_auth.password", "network_nfs_password_1",
		"ci_pincode_0", "token", "api_tokens", "ticket", "tmdb_api_key", "youtube_dev_id",
		"client_secret", "PASSWORD", "passwd"
	};
	for (size_t i = 0; i < sizeof(secretParams) / sizeof(secretParams[0]); i++) {
		INFO(secretParams[i]);
		CHECK(ylog::paramValue(secretParams[i], "S3NT1NEL") == "[redacted]");
	}
	// the password kept for another box goes by its number
	CHECK(ylog::paramValue("timer_remotebox_pass_0", "S3NT1NEL") == "[redacted]");
	CHECK(ylog::paramValue("timer_remotebox_pass_12", "S3NT1NEL") == "[redacted]");
	// an address keeps its place and loses its query's values and the user
	// and password in front of its host
	CHECK(ylog::url("http://u:S3NT1NEL@host:8080/list.m3u?key=S3NT1NEL&x=1")
	      == "http://[redacted]@host:8080/list.m3u?key=&x=");
	CHECK(ylog::url("http://host/a@b") == "http://host/a@b");
	CHECK(ylog::paramValue("webtv_source", "https://u:S3NT1NEL@host/l.m3u?key=S3NT1NEL")
	      == "https://[redacted]@host/l.m3u?key=");
	CHECK(ylog::headerValue("Referer", "http://box/page?token=S3NT1NEL") == "http://box/page?token=");
	// and settings that only look like one stay readable, as /control/config
	// has them
	const char *const plainParams[] = { "ac3_pass", "dts_pass", "timer_remotebox_pass_",
		"timer_remotebox_pass_x", "keyboard_layout", "name", "mode" };
	for (size_t i = 0; i < sizeof(plainParams) / sizeof(plainParams[0]); i++) {
		INFO(plainParams[i]);
		CHECK(ylog::paramValue(plainParams[i], "v\n") == "v\\n");
	}
}

TEST_CASE("a record of a request with line breaks in it stays one line without secrets", "[nhttpd]")
{
	CyhookHandler hh;
	hh.UrlData["clientaddr"] = "192.0.2.1\nINJECTED";
	hh.UrlData["startline"] = "GET /a\rINJECTED?password=S3NT1NEL&plain=v HTTP/1.1\nINJECTED";
	hh.UrlData["target"] = "/a\nINJECTED?password=S3NT1NEL&token=S3NT1NEL&plain=v\r\nINJECTED=1";
	hh.UrlData["fullurl"] = "/a\nINJECTED?password=S3NT1NEL";
	hh.HeaderList["Authorization"] = "Basic S3NT1NEL";
	hh.HeaderList["Cookie"] = "nhttpd-session=S3NT1NEL";
	hh.Method = M_GET;
	hh.httpStatus = HTTP_OK;

	const std::string records[] = { CmWebLog::recordCLF(&hh, 0), CmWebLog::recordELF(&hh, 0) };
	for (size_t i = 0; i < 2; i++) {
		INFO(records[i]);
		CHECK(count(records[i], '\n') == 1);
		CHECK(records[i][records[i].size() - 1] == '\n');
		CHECK(records[i].find('\r') == std::string::npos);
		CHECK(records[i].find("S3NT1NEL") == std::string::npos);
		// the names stay
		CHECK(records[i].find("password=") != std::string::npos);
		CHECK(records[i].find("plain=") != std::string::npos);
	}
}

namespace {

/* What the parser writes to the server's output at the highest log level
   while it reads request off a socket, and the two records of what it
   parsed. Watched: a parser that waits for ever ends the whole run. */
std::string parserLog(const std::string &request, std::string &clf, std::string &elf)
{
	int fd[2];
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
	CySocket *s = new CySocket(fd[0]);
	s->isOpened = true;
	s->isValid = true;
	REQUIRE(write(fd[1], request.data(), request.size()) == (ssize_t) request.size());
	close(fd[1]);

	char out[] = "/tmp/test_weblog_out_XXXXXX";
	const int captured = mkstemp(out);
	REQUIRE(captured >= 0);
	fflush(stdout);
	const int saved = dup(1);
	REQUIRE(saved >= 0);

	std::atomic<bool> done(false);
	std::thread watchdog([&done]() {
		for (int i = 0; i < 500 && !done; i++)
			usleep(10 * 1000);
		if (!done) {
			fprintf(stderr, "the parser did not finish a request in 5 s\n");
			_exit(1);
		}
	});

	const int level = CLogging::getInstance()->LogLevel;
	CLogging::getInstance()->LogLevel = 9;
	dup2(captured, 1);
	CWebserver ws;
	CWebserverConnection *con = new CWebserverConnection(&ws);
	con->Request.UrlData["clientaddr"] = "127.0.0.1";
	con->sock = s;
	con->Request.HandleRequest();
	fflush(stdout);
	dup2(saved, 1);
	close(saved);
	CLogging::getInstance()->LogLevel = level;
	done = true;
	watchdog.join();

	CyhookHandler hh;
	hh.UrlData = con->Request.UrlData;
	hh.HeaderList = con->Request.HeaderList;
	hh.ParamList = con->Request.ParameterList;
	hh.Method = con->Method;
	hh.httpStatus = HTTP_OK;
	clf = CmWebLog::recordCLF(&hh, 0);
	elf = CmWebLog::recordELF(&hh, 0);
	delete con;
	s->close();
	delete s;

	close(captured);
	const std::string text = readFile(out);
	unlink(out);
	return text;
}

std::string withLength(const std::string &head, const std::string &body)
{
	char len[32];
	snprintf(len, sizeof(len), "%lu", (unsigned long) body.size());
	return head + "Content-Length: " + len + "\r\n\r\n" + body;
}

} // namespace

TEST_CASE("the parser's debug lines carry no credential and no line of the request's own", "[nhttpd]")
{
	// shown is a line the case writes, which proves it was read at all;
	// one cut short at the end of the log's buffer still ends, so the next
	// line starts on its own
	struct Case { const char *what; std::string request; const char *shown; };
	const Case cases[] = {
		{ "headers and a query",
		  "GET /control/config?mod_auth_dot_password=S3NT1NEL&token=S3NT1NEL&plain=a%0AINJECTED"
		  "&src=http%3A%2F%2Fu%3AS3NT1NEL%40h%2Fl%3Fk%3DS3NT1NEL HTTP/1.1\r\n"
		  "Host: box\r\n"
		  "Referer: http://box/page?token=S3NT1NEL\r\n"
		  "Authorization: Basic S3NT1NEL\r\n"
		  "Cookie: nhttpd-session=S3NT1NEL\r\n"
		  "X-CSRF-Token: S3NT1NEL\r\n"
		  "X-Note: a\nINJECTED\r\n"
		  "X-A\nINJECTED-NAME: z\r\n\r\n",
		  "ParseHeader: name: Authorization value: [redacted]" },
		{ "a form",
		  withLength("POST /control/x HTTP/1.1\r\nHost: box\r\n"
			     "Content-Type: application/x-www-form-urlencoded\r\n",
			     "password=S3NT1NEL&ticket=S3NT1NEL&timer_remotebox_pass_0=S3NT1NEL"
			     "&plain=a%0D%0AINJECTED"),
		  "ParseParams: name: password value: 8 bytes" },
		{ "a query's values, whatever their names",
		  "GET /control/x?plain=V1S1BLE&current=S3NT1NEL HTTP/1.1\r\nHost: box\r\n\r\n",
		  "ParseParams: name: plain value: 7 bytes" },
		{ "a multipart field holding the current password",
		  withLength("POST /control/y HTTP/1.1\r\nHost: box\r\n"
			     "Content-Type: multipart/form-data; boundary=b\r\n",
			     "--b\r\nContent-Disposition: form-data; name=\"current\"\r\n\r\nS3NT1NEL\r\n--b--\r\n"),
		  "Parameter found. name:(current) value:([redacted])" },
		{ "a header longer than a log line",
		  "GET /index.html HTTP/1.1\r\nHost: box\r\n"
		  "X-Long: " + std::string(9000, 'x') + "\r\n"
		  "X-After: z\r\n\r\n",
		  "\n[yhttpd#8] ParseHeader: name: X-After value:  z\n" },
		{ "a multipart form",
		  withLength("POST /control/y HTTP/1.1\r\nHost: box\r\n"
			     "Content-Type: multipart/form-data; boundary=b\r\n",
			     "--b\r\nContent-Disposition: form-data; name=\"password\"\r\n\r\nS3NT1NEL\r\n--b--\r\n"),
		  "Parameter found. name:(password) value:([redacted])" },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		INFO(cases[i].what);
		std::string clf, elf;
		const std::string log = parserLog(cases[i].request, clf, elf);
		INFO(log);
		CHECK(log.find(cases[i].shown) != std::string::npos);
		CHECK(log.find("S3NT1NEL") == std::string::npos);
		CHECK(log.find("V1S1BLE") == std::string::npos);
		CHECK(markerNeverStartsALine(log, "INJECTED"));
		CHECK(clf.find("S3NT1NEL") == std::string::npos);
		CHECK(elf.find("S3NT1NEL") == std::string::npos);
		CHECK(count(clf, '\n') == 1);
		CHECK(count(elf, '\n') == 1);
	}
}

/* A file part names its field and its file on one line. The line is said by
   its length only, and the file's name as its field's value, redacted where
   the field's name says it holds a secret: here the password /control/config
   reads, sent as a file. */
TEST_CASE("a multipart file part is in the parser's debug lines without its secret", "[nhttpd]")
{
	// the reader writes the file where the box's upload goes; a file already
	// there is somebody else's, and the case leaves it alone
	if (access(UPLOAD_TMP_FILE, F_OK) == 0)
		return;
	const std::string body =
		"--b\r\nContent-Disposition: form-data; name=\"mod_auth_dot_password\"; filename=\"S3NT1NEL\"\r\n"
		"Content-Type: text/plain\r\n\r\nthe file\r\n--b--\r\n";
	const std::string request = withLength("POST /control/y HTTP/1.1\r\nHost: box\r\n"
					       "Content-Type: multipart/form-data; boundary=b\r\n", body);
	std::string clf, elf;
	const std::string log = parserLog(request, clf, elf);
	unlink(UPLOAD_TMP_FILE);
	INFO(log);
	CHECK(log.find("filename found. name:(mod_auth_dot_password) value:([redacted])") != std::string::npos);
	CHECK(log.find("this is a file!! found. line of ") != std::string::npos);
	CHECK(log.find("S3NT1NEL") == std::string::npos);
	CHECK(clf.find("S3NT1NEL") == std::string::npos);
	CHECK(elf.find("S3NT1NEL") == std::string::npos);
}

