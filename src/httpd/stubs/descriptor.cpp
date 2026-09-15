 *
 * Copyright (C) 2026 NI-Team

#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

namespace httpd
{

bool answerFromDescriptor(Response &r, int fd)
{
	if (fd < 0)
		return false;

	/* Off the descriptor and never off a name. The name a file was opened under
	   is a question about the directory and can be answered by a different file a
	   moment later, and on a box that directory is writable; the descriptor stays
	   the file that was opened however the name is rearranged underneath. A
	   length taken from the name would state the size of one file and send the
	   bytes of another. */
	struct stat st;
	std::memset(&st, 0, sizeof(st));
	if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0)
	{
		/* Given back here rather than left to the caller. A call that answers
		   both a failure and a descriptor the caller may still own is one where
		   the two answers can be read apart. */
		::close(fd);
		return false;
	}

	r.fd = fd;
	r.length = (uint64_t) st.st_size;
	return true;
}

} // namespace httpd
