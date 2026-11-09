//=============================================================================
// YHTTPD
// yprotect : the files that decide who may reach the box, which no answer
// hands out and no log is written into
//=============================================================================
#ifndef __yhttpd_yprotect_h__
#define __yhttpd_yprotect_h__

#include <string>
#include <vector>

namespace yprotect
{

// The files the program that runs the server keeps who may reach the box
// in, each with the working file it is written through: set before the
// server answers, by that program, which is the one that knows where they
// are. Until then, and in a server without one, there are none. What each
// name stands for now is kept from here on (coreapi::accessfiles), as is
// every such file a writer of the program makes; an empty list lets all of
// them go. False where one of the files there is could not be kept: it is
// still held back by its name, and a writer keeps it before it replaces it.
bool setFiles(const std::vector<std::string> &names);

// Whether the open descriptor fd may go out through no gate: a file no
// name reaches any more, which nothing can tell the content of; one of
// those files by device and inode against each name as it stands now with
// every link followed, so that a link, a second hard link or another
// spelling of the name is the same file; and a file kept since it was made
// (coreapi::accessfiles), so that one opened while it was being written or
// replaced is still one. A descriptor fstat cannot answer for, and a name
// that cannot be looked at for any reason but its absence, count as one.
bool isProtected(int fd);
// The same, where own_unnamed says the file is one the caller made itself and
// took the name of: having no name does not count against it then.
bool isProtected(int fd, bool own_unnamed);

// The places no answer is sent out of and no web folder may be, hold or lie
// in: the configuration directory first, which holds who may reach the box
// and the keys of the services it uses, then the kernel's trees /proc, /sys
// and /dev, whose files name what the program holds open.
std::vector<std::string> keptPlaces();
// Puts another list in their place, for the cases; an empty one puts the
// box's own back.
void setKeptPlacesForTest(const std::vector<std::string> &places);

// Whether the file fd is open on lies in one of those places, by the name
// /proc/self/fd gives it after every link, or where that name cannot be
// read or names no file of a directory. Asked of what was opened, so a link
// into one of the places from any web folder goes nowhere. config_ok leaves
// the configuration directory out, for the files the old pages' handlers name
// there (the channel lists) and the API's file answers, unless the start
// closed that directory (coreapi::accessfiles::closeTheirDirectory).
bool inKeptPlace(int fd, bool config_ok = false);

// Called by isProtected once it has looked at the name of that position in
// the list, before it looks at the next; NULL, as the server runs, for none.
// A case uses it to change a file between two of those looks.
void setLookedForTest(void (*looked)(size_t name));

// Whether path names one of those files by its name, whether or not it is
// there: the same last component in the same directory, however the
// directory is spelt. For a name about to be created, which has no inode to
// compare yet; what is opened under it is asked of isProtected all the same.
bool namesProtected(const std::string &path);

} // namespace yprotect

#endif // __yhttpd_yprotect_h__
