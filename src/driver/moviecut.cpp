/*
        Copyright (C) 2015 CoolStream International Ltd

        License: GPLv2

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation;

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <errno.h>
#include <math.h>
#include <utime.h>

#include <global.h>
#include <neutrino.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/helpbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>
#include <gui/components/cc.h>

#include <driver/screen_max.h>
#include <driver/moviecut.h>

#define PSI_SIZE 188*2
#define BUF_SIZE 1395*188
#define SAFE_GOP 1395*188
#define MP_TS_SIZE 262072 // ~0.5 sec
#define MINUTEOFFSET 117*262072
#define SECONDOFFSET MP_TS_SIZE*2

typedef struct pvr_file_info
{
	uint32_t  uDuration;      /* Time duration in Ms */
	uint32_t  uTSPacketSize;
} PVR_FILE_INFO;

struct mybook {
	off64_t pos;
	off64_t len;
	bool ok;
};

CMovieCut::CMovieCut()
{
	frameBuffer = CFrameBuffer::getInstance();
	timescale = NULL;
	percent = 0;
	int dx = 256;
	x = (((g_settings.screen_EndX- g_settings.screen_StartX)- dx) / 2) + g_settings.screen_StartX;
	y = g_settings.screen_EndY - 50;
}

CMovieCut::~CMovieCut()
{
	delete timescale;
}

void CMovieCut::paintProgress(bool refresh)
{
	if (!timescale) {
		timescale = new CProgressBar();
		timescale->setType(CProgressBar::PB_TIMESCALE);
	}
	if (refresh) {
		frameBuffer->paintBoxRel(x + 40, y+12, 200, 15, COL_INFOBAR_PLUS_0);//TODO: remove unneeded box paints
		timescale->reset();
	}
	timescale->setProgress(x + 41, y + 12, 200, 15, percent, 100);
	timescale->paint();
}

void CMovieCut::reset_atime(const char * path, time_t tt)
{
	struct utimbuf ut;
	ut.actime = tt-1;
	ut.modtime = tt-1;
	utime(path, &ut);
}

void CMovieCut::WriteHeader(const char * path, uint32_t duration)
{
	int srcfd = open(path, O_WRONLY | O_LARGEFILE);
	if (srcfd >= 0) {
		if (lseek64(srcfd, 188-sizeof(PVR_FILE_INFO), SEEK_SET) >= 0) {
			PVR_FILE_INFO pinfo;
			pinfo.uDuration = duration;
			pinfo.uTSPacketSize = 188;
			write(srcfd, (uint8_t *)&pinfo, sizeof(PVR_FILE_INFO));
		}
		close(srcfd);
	} else
		perror(path);
}

uint32_t CMovieCut::getHeaderDurationMS(MI_MOVIE_INFO * minfo)
{
	uint32_t duration = 0;
	int srcfd = open(minfo->file.Name.c_str(), O_RDONLY | O_LARGEFILE);
	if (srcfd >= 0) {
		if (lseek64(srcfd, 188-sizeof(PVR_FILE_INFO), SEEK_SET) >= 0) {
			PVR_FILE_INFO pinfo;
			memset(&pinfo, 0, sizeof(PVR_FILE_INFO));
			read(srcfd, (uint8_t *)&pinfo, sizeof(PVR_FILE_INFO));
			if (pinfo.uTSPacketSize == 188)
				duration = pinfo.uDuration;
			close(srcfd);
			printf("CMovieCut::%s: [%s] duration %d ms\n", __func__, minfo->file.Name.c_str(), duration);
		}
	} else
		perror(minfo->file.Name.c_str());

	return duration;
}

off64_t CMovieCut::getSecondSize(MI_MOVIE_INFO * minfo)
{
	struct stat64 s;
	if (stat64(minfo->file.Name.c_str(), &s)) {
		perror(minfo->file.Name.c_str());
		return 0;
	}
	uint32_t duration = getHeaderDurationMS(minfo);

	int len = minfo->length;
	if (len <= 0)
		len = 1;

	if (duration == 0)
		duration = len * 60 * 1000;

	off64_t mssize = ((float)s.st_size / (float)duration);
	printf("CMovieCut::%s: [%s] bytes per second: %" PRId64 "\n", __func__, minfo->file.Name.c_str(),  mssize*1000);
	return mssize*1000;
}

bool CMovieCut::truncateMovie(MI_MOVIE_INFO * minfo)
{
	off64_t secsize = getSecondSize(minfo);
	if (minfo->bookmarks.end == 0 || secsize == 0)
		return false;

	off64_t newsize = secsize * minfo->bookmarks.end;

	printf("CMovieCut::%s: [%s] truncate to %d sec, new size %" PRId64 "\n", __func__, minfo->file.Name.c_str(), minfo->bookmarks.end, newsize);
	if (truncate(minfo->file.Name.c_str(), newsize)) {
		perror(minfo->file.Name.c_str());
		return false;
	}
	minfo->file.Size = newsize;
	minfo->length = minfo->bookmarks.end/60;
	minfo->bookmarks.end = 0;
	reset_atime(minfo->file.Name.c_str(), minfo->file.Time);
	CMovieInfo cmovie;
	cmovie.saveMovieInfo(*minfo);
	WriteHeader(minfo->file.Name.c_str(), newsize/secsize*1000);
	return true;
}

int CMovieCut::check_pes_start (unsigned char *packet)
{
	// PCKT: 47 41 91 37 07 50 3F 14 BF 04 FE B9 00 00 01 EA 00 00 8C ...
	if (packet[0] == 0x47 &&                    // sync byte 0x47
			(packet[1] & 0x40))                     // pusi == 1
	{
		/* good, now we have to check if it is video stream */
		unsigned char *pes = packet + 4;
		if (packet[3] & 0x20)                   // adaptation field is present
			pes += packet[4] + 1;

		if (!memcmp(pes, "\x00\x00\x01", 3) && (pes[3] & 0xF0) == 0xE0) // PES start & video type
		{
			pes += 4;
			while (pes < (packet + 188 - 4))
				if (!memcmp(pes, "\x00\x00\x01\xB8", 4)) // GOP detect
					return 1;
				else
					pes++;
		}
	}
	return 0;
}

int CMovieCut::find_gop(unsigned char *buf, int r)
{
	for (int j = 0; j < r/188; j++) {
		if (check_pes_start(&buf[188*j]))
			return 188*j;
	}
	return -1;
}

off64_t CMovieCut::fake_read(int fd, unsigned char *buf, size_t size, off64_t fsize)
{
	off64_t cur = lseek64(fd, 0, SEEK_CUR);

	buf[0] = 0x47;
	if ((cur + (off64_t)size) > fsize)
		return(fsize - cur);
	else
		return size;
}

int CMovieCut::read_psi(const char * spart, unsigned char * buf)
{
	int srcfd = open(spart, O_RDONLY | O_LARGEFILE);
	if (srcfd >= 0) {
		/* read psi */
		int r = read(srcfd, buf, PSI_SIZE);
		close(srcfd);
		if (r != PSI_SIZE) {
			perror("read psi");
			return -1;
		}
		return 0;
	}
	return -1;
}

void CMovieCut::save_info(MI_MOVIE_INFO * minfo, char * dpart, off64_t spos, off64_t secsize)
{
	CMovieInfo cmovie;
	MI_MOVIE_INFO ninfo = *minfo;
	ninfo.file.Name = dpart;
	ninfo.file.Size = spos;
	ninfo.length = spos/secsize/60;
	ninfo.bookmarks.end = 0;
	ninfo.bookmarks.start = 0;
	ninfo.bookmarks.lastPlayStop = 0;
	for (int book_nr = 0; book_nr < MI_MOVIE_BOOK_USER_MAX; book_nr++) {
		if (ninfo.bookmarks.user[book_nr].pos != 0 && ninfo.bookmarks.user[book_nr].length > 0) {
			ninfo.bookmarks.user[book_nr].pos = 0;
			ninfo.bookmarks.user[book_nr].length = 0;
		}
	}
	cmovie.saveMovieInfo(ninfo);
	WriteHeader(ninfo.file.Name.c_str(), spos/secsize*1000);
	reset_atime(dpart, minfo->file.Time);
}

void CMovieCut::findNewName(const char * fname, char * dpart, size_t dpart_len)
{
	char npart[255];
	snprintf(npart, sizeof(npart), "%s", fname);
	char * ptr = strstr(npart+strlen(npart)-3, ".ts");
	if (ptr)
		*ptr = 0;

	struct stat64 s;
	int dp = 0;
	snprintf(dpart, dpart_len, "%s_%d.ts", npart, dp);
	while (!stat64(dpart, &s))
		snprintf(dpart, dpart_len, "%s_%d.ts", npart, ++dp);
}

int CMovieCut::compare_book(const void *x, const void *y)
{
	struct mybook *px = (struct mybook*) x;
	struct mybook *py = (struct mybook*) y;
	int dx = px->pos / (off64_t) 1024;
	int dy = py->pos / (off64_t) 1024;
	int res = dx - dy;
	//printf("SORT: %lld and %lld res %d\n", px->pos, py->pos, res);
	return res;
}

int CMovieCut::getInput()
{
	neutrino_msg_data_t data;
	neutrino_msg_t msg;
	int retval = 0;
	g_RCInput->getMsg(&msg, &data, 1, false);
	if (msg == CRCInput::RC_home) {
		if (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_MOVIECUT_CANCEL, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes)
			retval |= 4;
	}
	if (msg != CRCInput::RC_timeout)
		retval |= 1;
	if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
		retval |= 2;
//printf("input: msg %d (%x) ret %d\n", msg, msg, retval);
	return retval;
}

bool CMovieCut::cutMovie(MI_MOVIE_INFO * minfo)
{
	struct mybook books[MI_MOVIE_BOOK_USER_MAX+2];
	unsigned char psi[PSI_SIZE];
	char dpart[255];
	int bcount = 0;
	int dstfd = -1, srcfd = -1;
	struct stat64 s;
	off64_t spos = 0;
	bool need_gop = 0;
	int was_cancel = 0;
	bool retval = false;
	time_t tt = time(0);
	time_t tt1;

	off64_t size = minfo->file.Size;
	off64_t secsize = getSecondSize(minfo);
	off64_t newsize = size;

	if (minfo->bookmarks.start != 0) {
		books[bcount].pos = 0;
		books[bcount].len = (minfo->bookmarks.start * secsize)/188 * 188;
		if (books[bcount].len > SAFE_GOP)
			books[bcount].len -= SAFE_GOP;
		books[bcount].ok = 1;
		printf("CMovieCut::%s: start bookmark %d at %" PRId64 " len %" PRId64 "\n", __func__, bcount, books[bcount].pos, books[bcount].len);
		bcount++;
	}
	for (int book_nr = 0; book_nr < MI_MOVIE_BOOK_USER_MAX; book_nr++) {
		if (minfo->bookmarks.user[book_nr].pos != 0 && minfo->bookmarks.user[book_nr].length > 0) {
			books[bcount].pos = (minfo->bookmarks.user[book_nr].pos * secsize)/188 * 188;
			books[bcount].len = (minfo->bookmarks.user[book_nr].length * secsize)/188 * 188;
			if (books[bcount].len > SAFE_GOP)
				books[bcount].len -= SAFE_GOP;
			books[bcount].ok = 1;
			printf("CMovieCut::%s: jump bookmark %d at %" PRId64 " len %" PRId64 " -> skip to %" PRId64 "\n", __func__, bcount, books[bcount].pos, books[bcount].len, books[bcount].pos+books[bcount].len);
			bcount++;
		}
	}
	if (minfo->bookmarks.end != 0) {
		books[bcount].pos = ((off64_t) minfo->bookmarks.end * secsize)/188 * 188;
		books[bcount].len = size - books[bcount].pos;
		books[bcount].ok = 1;
		printf("CMovieCut::%s: end bookmark %d at %" PRId64 "\n", __func__, bcount, books[bcount].pos);
		bcount++;
	}

	if (!bcount)
		return false;

	unsigned char * buf = new unsigned char[BUF_SIZE];
	if (buf == 0) {
		perror("new");
		return false;
	}

	paintProgress(true);

	qsort(books, bcount, sizeof(struct mybook), compare_book);
	for (int i = 0; i < bcount; i++) {
		if (books[i].ok) {
			//printf("cut: bookmark %d at %" PRId64 " len %" PRId64 " -> skip to %" PRId64 "\n", i, books[i].pos, books[i].len, books[i].pos+books[i].len);
			newsize -= books[i].len;
			off64_t curend = books[i].pos + books[i].len;
			/* check for overlapping bookmarks */
			for (int j = i + 1; j < bcount; j++) {
				if ((books[j].pos > books[i].pos) && (books[j].pos < curend)) {
					off64_t newend = books[j].pos + books[j].len;
					if (newend > curend) {
						printf("CMovieCut::%s: bad bookmark %d, position %" PRId64 " len %" PRId64 ", adjusting..\n", __func__, j, books[j].pos, books[j].len);
						books[j].pos = curend;
						books[j].len = newend - curend;
					} else {
						printf("CMovieCut::%s: bad bookmark %d, position %" PRId64 " len %" PRId64 ", skipping..\n", __func__, j, books[j].pos, books[j].len);
						books[j].ok = 0;
					}
				}
			}
		}
	}
	findNewName(minfo->file.Name.c_str(), dpart, sizeof(dpart));

	int bindex = 0;
	off64_t bpos = books[bindex].pos;
	off64_t bskip = books[bindex].len;
	off64_t offset = 0;

	printf("CMovieCut::%s: new file %s, expected size %" PRId64 ", start time %s", __func__, dpart, newsize, ctime(&tt));
	dstfd = open(dpart, O_CREAT|O_WRONLY|O_TRUNC| O_LARGEFILE, 0644);
	if (dstfd < 0) {
		perror(dpart);
		goto ret_err;
	}
	if (read_psi(minfo->file.Name.c_str(), &psi[0])) {
		perror(minfo->file.Name.c_str());
		goto ret_err;
	}
	write(dstfd, psi, PSI_SIZE);

	stat64(minfo->file.Name.c_str(), &s);

	srcfd = open(minfo->file.Name.c_str(), O_RDONLY | O_LARGEFILE);
	if (srcfd < 0) {
		perror(minfo->file.Name.c_str());
		goto ret_err;
	}
	lseek64(srcfd, offset, SEEK_SET);

	/* process all bookmarks */
	while (true) {
		off64_t until = bpos;
		printf("CMovieCut::%s: bookmark #%d reading from %" PRId64 " to %" PRId64 " (%" PRId64 ") want gop %d\n", __func__, bindex, offset, until, until - offset, need_gop);
		/* read up to jump end */
		while (offset < until) {
			int msg = getInput();
			was_cancel = msg & 2;
			if (msg & 4) {
				unlink(dpart);
				retval = true;
				goto ret_err;
			}
			size_t toread = (until-offset) > BUF_SIZE ? BUF_SIZE : until - offset;
			size_t r = read(srcfd, buf, toread);
			if (r > 0) {
				int wptr = 0;
				if (r != toread) 
					printf("CMovieCut::%s: short read at %" PRId64 ": %d\n", __func__, offset, (int)r);
				if (buf[0] != 0x47)
					printf("CMovieCut::%s: buffer not aligned at %" PRId64 "\n", __func__, offset);
				if (need_gop) {
					int gop = find_gop(buf, r);
					if (gop >= 0) {
						printf("CMovieCut::%s: GOP found at %" PRId64 " offset %d\n", __func__, (off64_t)(offset+gop), gop);
						newsize -= gop;
						wptr = gop;
					} else
						printf("CMovieCut::%s: GOP not found\n", __func__);
					need_gop = 0;
				}
				offset += r;
				spos += r - wptr;
				percent = (int) ((float)(spos)/(float)(newsize)*100.);
				paintProgress(msg != 0);
				size_t wr = write(dstfd, &buf[wptr], r-wptr);
				if (wr < (r-wptr)) {
					perror(dpart);
					goto ret_err;
				}
			} else if (offset < s.st_size) {
				/* read error ? */
				perror(minfo->file.Name.c_str());
				goto ret_err;
			}
		}
		printf("CMovieCut::%s: current file pos %" PRId64 " write pos %" PRId64 " book pos %" PRId64 " next offset %" PRId64 "\n", __func__, offset, spos, bpos, bpos + bskip);
		need_gop = 1;
		offset = bpos + bskip;

		bindex++;
		while(bindex < bcount) {
			if(books[bindex].ok)
				break;
			else
				bindex++;
		}
		if(bindex < bcount) {
			bpos = books[bindex].pos;
			bskip = books[bindex].len;
		} else
			bpos = size;

		if (offset >= s.st_size) {
			printf("CMovieCut::%s: offset behind EOF: %" PRId64 " from %" PRId64 "\n", __func__, offset, s.st_size);
			break;
		}
		lseek64(srcfd, offset, SEEK_SET);
	}
	tt1 = time(0);
	printf("CMovieCut::%s: total written %" PRId64 " tooks %ld secs end time %s", __func__, spos, tt1-tt, ctime(&tt1));

	save_info(minfo, dpart, spos, secsize);
	retval = true;
ret_err:
	if (srcfd >= 0)
		close(srcfd);
	if (dstfd >= 0)
		close(dstfd);

	delete [] buf;
	if (was_cancel)
		g_RCInput->postMsg(CRCInput::RC_home, 0);

	frameBuffer->paintBoxRel(x + 40, y+12, 200, 15, COL_INFOBAR_PLUS_0);//TODO: remove unneeded box paints
	return retval;
}

bool CMovieCut::copyMovie(MI_MOVIE_INFO * minfo, bool onefile)
{
	struct mybook books[MI_MOVIE_BOOK_USER_MAX+2];
	struct stat64 s;
	char dpart[255];
	unsigned char psi[PSI_SIZE];
	int dstfd = -1, srcfd = -1;
	off64_t spos = 0, btotal = 0;
	time_t tt = time(0);
	bool need_gop = 0;
	bool dst_done = 0;
	bool was_cancel = false;
	bool retval = false;
	int bcount = 0;
	off64_t newsize = 0;

	off64_t secsize = getSecondSize(minfo);
	for (int book_nr = 0; book_nr < MI_MOVIE_BOOK_USER_MAX; book_nr++) {
		if (minfo->bookmarks.user[book_nr].pos != 0 && minfo->bookmarks.user[book_nr].length > 0) {
			books[bcount].pos = (minfo->bookmarks.user[book_nr].pos * secsize)/188 * 188;
			if (books[bcount].pos > SAFE_GOP)
				books[bcount].pos -= SAFE_GOP;
			books[bcount].len = (minfo->bookmarks.user[book_nr].length * secsize)/188 * 188;
			books[bcount].ok = 1;
			printf("copy: jump bookmark %d at %" PRId64 " len %" PRId64 "\n", bcount, books[bcount].pos, books[bcount].len);
			newsize += books[bcount].len;
			bcount++;
		}
	}

	if (!bcount)
		return false;

	unsigned char * buf = new unsigned char[BUF_SIZE];
	if (buf == 0) {
		perror("new");
		return false;
	}

	paintProgress(true);

	printf("********* %d boormarks, to %s file(s), expected size to copy %" PRId64 ", start time %s", bcount, onefile ? "one" : "many", newsize, ctime(&tt));
	if (read_psi(minfo->file.Name.c_str(), &psi[0])) {
		perror(minfo->file.Name.c_str());
		goto ret_err;
	}
	srcfd = open(minfo->file.Name.c_str(), O_RDONLY | O_LARGEFILE);
	if (srcfd < 0) {
		perror(minfo->file.Name.c_str());
		goto ret_err;
	}
	stat64(minfo->file.Name.c_str(), &s);
	for (int i = 0; i < bcount; i++) {
		printf("\ncopy: processing bookmark %d at %" PRId64 " len %" PRId64 "\n", i, books[i].pos, books[i].len);

		if (!dst_done || !onefile) {
			findNewName(minfo->file.Name.c_str(), dpart, sizeof(dpart));
			dstfd = open(dpart, O_CREAT|O_WRONLY|O_TRUNC| O_LARGEFILE, 0644);
			printf("copy: new file %s fd %d\n", dpart, dstfd);
			if (dstfd < 0) {
				printf("failed to open %s\n", dpart);
				goto ret_err;
			}
			dst_done = 1;
			spos = 0;
			write(dstfd, psi, PSI_SIZE);
		}
		need_gop = 1;

		off64_t offset = books[i].pos;
		lseek64(srcfd, offset, SEEK_SET);
		off64_t until = books[i].pos + books[i].len;
		printf("copy: read from %" PRId64 " to %" PRId64 " read size %d want gop %d\n", offset, until, BUF_SIZE, need_gop);
		while (offset < until) {
			size_t toread = (until-offset) > BUF_SIZE ? BUF_SIZE : until - offset;
			int msg = getInput();
			was_cancel = msg & 2;
			if (msg & 4) {
				unlink(dpart);
				retval = true;
				goto ret_err;
			}
			size_t r = read(srcfd, buf, toread);
			if (r > 0) {
				int wptr = 0;
				if (r != toread)
					printf("****** short read ? %d\n", (int)r);
				if (buf[0] != 0x47)
					printf("copy: buffer not aligned at %" PRId64 "\n", offset);
				if (need_gop) {
					int gop = find_gop(buf, r);
					if (gop >= 0) {
						printf("cut: GOP found at %" PRId64 " offset %d\n", (off64_t)(offset+gop), gop);
						newsize -= gop;
						wptr = gop;
					} else
						printf("cut: GOP needed, but not found\n");
					need_gop = 0;
				}
				offset += r;
				spos += r - wptr;
				btotal += r;
				percent = (int) ((float)(btotal)/(float)(newsize)*100.);
				paintProgress(msg != 0);

				size_t wr = write(dstfd, &buf[wptr], r-wptr);
				if (wr < (r-wptr)) {
					printf("write to %s failed\n", dpart);
					unlink(dpart);
					goto ret_err;
				}
			} else if (offset < s.st_size) {
				/* read error ? */
				perror(minfo->file.Name.c_str());
				break;
			}
		} /* while(offset < until) */

		if (!onefile) {
			close(dstfd);
			dstfd = -1;
			save_info(minfo, dpart, spos, secsize);
			time_t tt1 = time(0);
			printf("copy: ********* %s: total written %" PRId64 " took %ld secs\n", dpart, spos, tt1-tt);
		}
	} /* for all books */
	if (onefile) {
		save_info(minfo, dpart, spos, secsize);
		time_t tt1 = time(0);
		printf("copy: ********* %s: total written %" PRId64 " took %ld secs\n", dpart, spos, tt1-tt);
	}
	retval = true;
ret_err:
	if (srcfd >= 0)
		close(srcfd);
	if (dstfd >= 0)
		close(dstfd);
	delete [] buf;
	if (was_cancel)
		g_RCInput->postMsg(CRCInput::RC_home, 0);
	frameBuffer->paintBoxRel(x + 40, y+12, 200, 15, COL_INFOBAR_PLUS_0);//TODO: remove unneeded box paints
	return retval;
}
