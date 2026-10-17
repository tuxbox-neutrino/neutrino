//=============================================================================
// YHTTPD
// ystatic : where a request for a file ends up, and whether it may be
// answered without a login
//=============================================================================
#ifndef __yhttpd_ystatic_h__
#define __yhttpd_ystatic_h__

#include <string>

class CyhookHandler;

namespace ystatic
{

// Where the file a request names lies, in the order the server has always
// looked: the override directory, then the web directory, each also as .gz,
// then, with Y_CONFIG_FEATUE_SENDFILE_CAN_ACCESS_ALL, the name as it stands
// anywhere on the box.
struct Target
{
	enum Origin { NONE, CORE, OVERRIDE, RAW };
	Origin origin;
	std::string file;	// the file to open, as found
	bool fallback;		// the web directory's index.html for a page path
	Target() : origin(NONE), fallback(false) {}
};

// path and filename as the request's UrlData holds them.
Target resolveName(CyhookHandler *hh, const std::string &path, const std::string &filename);

// The request's own file, or, with WebsiteMain.spa_fallback switched on, the
// web directory's index.html for a GET or HEAD whose last path segment has
// no file extension (also one ending in /), that names no file and no
// reserved space: a page of a single-page interface reloaded at its own
// address.
Target resolve(CyhookHandler *hh);

// Whether the decoded URL lies in a space another hook answers or that is
// never open: /api, /control, /cgi-bin, /fb, /y, /hosted, /extensions,
// /login and /logout, each as itself or with anything below it.
bool inReservedSpace(const std::string &url);

// A template the parser runs or a script, by its name (any case, also as
// .gz): such a file is never handed out without a login.
bool templateOrScript(const std::string &name);

// Whether the web directory has its own sign-in page, /login.html, that goes
// out without a login (openPublic). Only to that page are browsers sent to
// sign in: a web directory without one (another interface in its place), or
// with only a copy in the override directory, must not send them to a page
// that is itself behind the login.
bool ownLoginPage(CyhookHandler *hh);

// Whether the page the descriptor fd holds (opened by openPublic for target
// t) waits for the login all the same: an HTML page, by the name asked for
// or the file reached, of a web directory that has a login.html of its own
// (in the directory itself; a copy in the override directory keeps the pages
// behind the login), other than the file the sign-in page goes out from. The
// pages of this interface open once somebody signed in, as they did while
// they were templates; their scripts, styles, texts and pictures stay open,
// and so does every file of another interface in the web directory's place,
// which signs in by itself.
bool pageNeedsLogin(CyhookHandler *hh, const Target &t, int fd);

// Whether the opened descriptor fd is a regular file of the web directory:
// inside it as /proc/self/fd names it after every link, not under its
// extensions/, and not unlinked since (an unlinked file has no name left
// that tells what it is). below is that name below the web directory.
bool ownOpened(CyhookHandler *hh, int fd, std::string &below);

// Opens the target and answers the descriptor when the opened file may go
// out without a login, else -1. What is checked is the file actually
// opened, through /proc/self/fd, and not the name: a regular file that came
// from the web directory, lies within it after every link is followed, not
// under its extensions/ (ownOpened), and is neither template nor script. A
// FIFO is not waited on. The caller sends from this descriptor and never
// opens the name again, so a file swapped for a link after the check is not
// what goes out. Without /proc (a server in a chroot) nothing is open.
int openPublic(CyhookHandler *hh, const Target &t);

// The name as a URL the server's second analyzeURL pass turns back into
// exactly this name: that pass decodes %XX and + and cuts at ?.
std::string reparseSafe(const std::string &name);

} // namespace ystatic

#endif // __yhttpd_ystatic_h__
