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
	Target() : origin(NONE) {}
};

// path and filename as the request's UrlData holds them.
Target resolveName(CyhookHandler *hh, const std::string &path, const std::string &filename);

// The request's own file.
Target resolve(CyhookHandler *hh);

// Whether the decoded URL lies in a space another hook answers or that is
// never open: /api, /control, /cgi-bin, /fb, /y, /hosted, /extensions,
// /login and /logout, each as itself or with anything below it.
bool inReservedSpace(const std::string &url);

// A template the parser runs or a script, by its name (any case, also as
// .gz): such a file is never handed out without a login.
bool templateOrScript(const std::string &name);

// The web directory's own sign-in page, opened: /login.yhtm found there
// and not in the override directory (the parser would run that copy
// unasked), and the regular file login.yhtm of the web directory itself,
// not a link to another page, a compressed copy, or out of the directory.
// Answers the descriptor or -1. Only that page is open while it is a
// template, and the parser reads it from this descriptor (PublicFd), not
// by its name again.
int openLoginPage(CyhookHandler *hh);

// Whether openLoginPage finds one. Only to that page are browsers sent to
// sign in: a web directory without one (another interface in its place)
// must not send them to a page that is itself behind the login.
bool ownLoginPage(CyhookHandler *hh);

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
