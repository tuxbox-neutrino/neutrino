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
// in, each with the working file it is written through: set once, before
// the server answers, by that program, which is the one that knows where
// they are. Until then, and in a server without one, there are none.
void setFiles(const std::vector<std::string> &names);

// Whether the open descriptor fd holds one of those files, by device and
// inode against each name as it stands now with every link followed, so
// that a link, a second hard link or another spelling of the name is the
// same file. A descriptor fstat cannot answer for counts as one.
bool isProtected(int fd);

// Whether path names one of those files by its name, whether or not it is
// there: the same last component in the same directory, however the
// directory is spelt. For a name about to be created, which has no inode to
// compare yet; what is opened under it is asked of isProtected all the same.
bool namesProtected(const std::string &path);

} // namespace yprotect

#endif // __yhttpd_yprotect_h__
