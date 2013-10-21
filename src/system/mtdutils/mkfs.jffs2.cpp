/*
 * Build a JFFS2 image in a file, from a given directory tree.
 *
 * Copyright 2001, 2002 Red Hat, Inc.
 *	   2001 David A. Schleef <ds@lineo.com>
 *	   2002 Axis Communications AB
 *	   2001, 2002 Erik Andersen <andersen@codepoet.org>
 *	   2004 University of Szeged, Hungary
 *	   2006 KaiGai Kohei <kaigai@ak.jp.nec.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Cross-endian support added by David Schleef <ds@schleef.org>.
 *
 * Major architectural rewrite by Erik Andersen <andersen@codepoet.org>
 * to allow support for making hard links (though hard links support is
 * not yet implemented), and for munging file permissions and ownership
 * on the fly using --faketime, --squash, --devtable.   And I plugged a
 * few memory leaks, adjusted the error handling and fixed some little
 * nits here and there.
 *
 * I also added a sample device table file.  See device_table.txt
 *  -Erik, September 2001
 *
 * Cleanmarkers support added by Axis Communications AB
 *
 * Rewritten again.  Cleanly separated host and target filsystem
 * activities (mainly so I can reuse all the host handling stuff as I
 * rewrite other mkfs utils).  Added a verbose option to list types
 * and attributes as files are added to the file system.  Major cleanup
 * and scrubbing of the code so it can be read, understood, and
 * modified by mere mortals.
 *
 *  -Erik, November 2002
 */

#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <libgen.h>
#include <ctype.h>
#include <time.h>
#include <byteswap.h>
#include <crc32.h>
#include <inttypes.h>

#include <string>

#include "rbtree.h"
#include <system/helpers.h>
#include <system/localize.h>

extern CLocaleManager *g_Locale;

#define PROGRAM_NAME "mkfs.jffs2"

int page_size = -1;
int target_endian = __BYTE_ORDER;
struct rb_root hardlinks;
struct filesystem_entry {
	char *name;				/* Name of this directory (think basename) */
	char *path;				/* Path of this directory (think dirname) */
	char *fullname;				/* Full name of this directory (i.e. path+name) */
	char *hostname;				/* Full path to this file on the host filesystem */
	uint32_t ino;				/* Inode number of this file in JFFS2 */
	struct stat sb;				/* Stores directory permissions and whatnot */
	char *link;				/* Target a symlink points to. */
	struct filesystem_entry *parent;	/* Parent directory */
	struct filesystem_entry *prev;		/* Only relevant to non-directories */
	struct filesystem_entry *next;		/* Only relevant to non-directories */
	struct filesystem_entry *files;		/* Only relevant to directories */
	struct rb_node hardlink_rb;
};
struct filesystem_entry *fse_root = NULL;

#include "common.h"
#include "mtd/jffs2-user.h"

struct jffs2_unknown_node cleanmarker;

#include "compr.h"

#include "mkfs.jffs2.h"
#include "sumtool.h"

/* Do not use the weird XPG version of basename */
#undef basename

//#define DMALLOC
//#define mkfs_debug_msg    errmsg
#define mkfs_debug_msg(a...)	{ }

#define PAD(x) (((x)+3)&~3)

#define JFFS2_MAX_FILE_SIZE 0xFFFFFFFF
#ifndef JFFS2_MAX_SYMLINK_LEN
#define JFFS2_MAX_SYMLINK_LEN 254
#endif

CMkfsJFFS2::CMkfsJFFS2()
{
	Init();
}

CMkfsJFFS2::~CMkfsJFFS2()
{
	classClear();
}

bool CMkfsJFFS2::classInit()
{
	if (useSumtool_) {
		sumName_ = imageName_;
		imageName_ += ".tmp";
	}

	out_fd = open(imageName_.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
	if (out_fd == -1) {
		sys_errmsg("open output file: %s", imageName_.c_str());
		return false;
	}
	jffs2_compressors_init();
	compressorIsInit = true;
	return true;
}

void CMkfsJFFS2::classClear()
{
	if (fse_root != NULL) {
		cleanup(fse_root);
		fse_root = NULL;
	}
	if (out_fd > 0) {
		close(out_fd);
		out_fd = -1;
	}
	if (compressorIsInit) {
		jffs2_compressors_exit();
		compressorIsInit = false;
	}
}

void CMkfsJFFS2::Init()
{
	out_fd			= -1;
	squash_uids		= 0;
	squash_perms		= 0;
	fake_times		= 0;
	target_endian		= __BYTE_ORDER;
	all_read		= 0;
	printProgress		= 0;
	kbUsed			= 0;
	progressBar		= NULL;

	ino			= 0;
	out_ofs			= 0;
	erase_block_size	= 65536;
	pad_fs_size		= 0;
	add_cleanmarkers	= 0;
	cleanmarker_size	= sizeof(cleanmarker);
	compressorIsInit	= false;
	fse_root		= NULL;
	imageName_		= "";
	sumName_		= "";
	useSumtool_		= true;

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size < 0) /* System doesn't know so ... */
		page_size = 4096; /* ... we make an educated guess */

	memset(ffbuf, 0xFF, sizeof(ffbuf));
}

char *CMkfsJFFS2::xreadlink(const char *path)
{
	static const int GROWBY = 80; /* how large we will grow strings by */

	char *buf = NULL;
	int bufsize = 0, readsize = 0;

	do {
		buf = (char*)xrealloc(buf, bufsize += GROWBY);
		readsize = readlink(path, buf, bufsize); /* 1st try */
		if (readsize == -1) {
			sys_errmsg("%s:%s", PROGRAM_NAME, path);
			return NULL;
		}
	} while (bufsize < readsize + 1);

	buf[readsize] = '\0';

	return buf;
}

void CMkfsJFFS2::cleanup(struct filesystem_entry *dir)
{
	struct filesystem_entry *e;

	e = dir->files;
	while (e) {
		if (e->name)
			free(e->name);
		if (e->path)
			free(e->path);
		if (e->fullname)
			free(e->fullname);
		e->name = NULL;
		e->path = NULL;
		e->fullname = NULL;
		e->prev = NULL;
		filesystem_entry *prev = e;
		if (S_ISDIR(e->sb.st_mode)) {
			cleanup(e);
		}
		e = e->next;
		free(prev);
	}
}

struct filesystem_entry *CMkfsJFFS2::add_host_filesystem_entry(const char *name,
					const char *path, unsigned long uid, unsigned long gid,
					unsigned long mode, dev_t rdev, struct filesystem_entry *parent) {
	int status;
	char *tmp;
	struct stat sb;
	time_t timestamp = time(NULL);
	struct filesystem_entry *entry;

	memset(&sb, 0, sizeof(struct stat));
	status = lstat(path, &sb);

	if (status >= 0) {
		/* It is ok for some types of files to not exit on disk (such as
		 * device nodes), but if they _do_ exist the specified mode had
		 * better match the actual file or strange things will happen.... */
		if ((mode & S_IFMT) != (sb.st_mode & S_IFMT)) {
			sys_errmsg ("%s: file type does not match specified type!", path);
		}
		timestamp = sb.st_mtime;
	} else {
		/* If this is a regular file, it _must_ exist on disk */
		if ((mode & S_IFMT) == S_IFREG) {
			sys_errmsg("%s: does not exist!", path);
		}
	}

	/* Squash all permissions so files are owned by root, all
	 * timestamps are _right now_, and file permissions
	 * have group and other write removed */
	if (squash_uids) {
		uid = gid = 0;
	}
	if (squash_perms) {
		if (!S_ISLNK(mode)) {
			mode &= ~(S_IWGRP | S_IWOTH);
			mode &= ~(S_ISUID | S_ISGID);
		}
	}
	if (fake_times) {
		timestamp = 0;
	}

	entry = reinterpret_cast<filesystem_entry*>(xcalloc(1, sizeof(struct filesystem_entry)));

	entry->hostname = xstrdup(path);
	entry->fullname = xstrdup(name);
	tmp = xstrdup(name);
	entry->name = xstrdup(basename(tmp));
	free(tmp);
	tmp = xstrdup(name);
	entry->path = xstrdup(dirname(tmp));
	free(tmp);

	entry->sb.st_ino = sb.st_ino;
	entry->sb.st_dev = sb.st_dev;
	entry->sb.st_nlink = sb.st_nlink;

	entry->sb.st_uid = uid;
	entry->sb.st_gid = gid;
	entry->sb.st_mode = mode;
	entry->sb.st_rdev = rdev;
	entry->sb.st_atime = entry->sb.st_ctime =
				     entry->sb.st_mtime = timestamp;
	if (S_ISREG(mode)) {
		entry->sb.st_size = sb.st_size;
	}
	if (S_ISLNK(mode)) {
		entry->link = xreadlink(path);
		entry->sb.st_size = strlen(entry->link);
	}

	/* This happens only for root */
	if (!parent)
		return (entry);

	/* Hook the file into the parent directory */
	entry->parent = parent;
	if (!parent->files) {
		parent->files = entry;
	} else {
		struct filesystem_entry *prev;
		for (prev = parent->files; prev->next; prev = prev->next);
		prev->next = entry;
		entry->prev = prev;
	}

	return (entry);
}

struct filesystem_entry *CMkfsJFFS2::find_filesystem_entry(
				struct filesystem_entry *dir, char *fullname, uint32_t type) {
	struct filesystem_entry *e = dir;

	if (S_ISDIR(dir->sb.st_mode)) {
		/* If this is the first call, and we actually want this
		 * directory, then return it now */
		if (strcmp(fullname, e->fullname) == 0)
			return e;

		e = dir->files;
	}
	while (e) {
		if (S_ISDIR(e->sb.st_mode)) {
			int len = strlen(e->fullname);

			/* Check if we are a parent of the correct path */
			if (strncmp(e->fullname, fullname, len) == 0) {
				/* Is this an _exact_ match? */
				if (strcmp(fullname, e->fullname) == 0) {
					return (e);
				}
				/* Looks like we found a parent of the correct path */
				if (fullname[len] == '/') {
					if (e->files) {
						return (find_filesystem_entry (e, fullname, type));
					} else {
						return NULL;
					}
				}
			}
		} else {
			if (strcmp(fullname, e->fullname) == 0) {
				return (e);
			}
		}
		e = e->next;
	}
	return (NULL);
}

void CMkfsJFFS2::printProgressData(bool finish)
{
	static int p_old = -1;
	if (finish) p_old = -1;
	int p = (finish) ? 100 : ((all_read/1024)*100 )/kbUsed;
	if (p != p_old) {
		p_old = p;
		printf("mkfs.jffs2: %ld KB from %ld KB read (%d%%)\n", all_read / 1024, kbUsed, p); fflush(stdout);
	}
}

void CMkfsJFFS2::paintProgressBar(bool finish)
{
	static int p_old = -1;
	if (finish) p_old = -1;
	int p = (finish) ? 100 : ((all_read/1024)*100 )/kbUsed;
	if (p != p_old) {
		p_old = p;
		progressBar->showLocalStatus(p);
	}
}

unsigned int CMkfsJFFS2::write_regular_file(struct filesystem_entry *e)
{
	int fd, len;
	uint32_t ver;
	unsigned int offset;
	unsigned char *buf, *cbuf, *wbuf;
	struct jffs2_raw_inode ri;
	struct stat *statbuf;
	unsigned int totcomp = 0;

	statbuf = &(e->sb);
	if ((uint32_t)statbuf->st_size >= JFFS2_MAX_FILE_SIZE) {
		errmsg("Skipping file \"%s\" too large.", e->path);
		return -1;
	}
	fd = open(e->hostname, O_RDONLY);
	if (fd == -1) {
		sys_errmsg("%s: open file", e->hostname);
	}

	e->ino = ++ino;
	mkfs_debug_msg("writing file '%s'  ino=%lu  parent_ino=%lu",
		       e->name, (unsigned long) e->ino,
		       (unsigned long) e->parent->ino);
	write_dirent(e);

	buf = (unsigned char*)xmalloc(page_size);
	cbuf = NULL;

	ver = 0;
	offset = 0;

	memset(&ri, 0, sizeof(ri));
	ri.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	ri.nodetype = cpu_to_je16(JFFS2_NODETYPE_INODE);

	ri.ino = cpu_to_je32(e->ino);
	ri.mode = cpu_to_jemode(statbuf->st_mode);
	ri.uid = cpu_to_je16(statbuf->st_uid);
	ri.gid = cpu_to_je16(statbuf->st_gid);
	ri.atime = cpu_to_je32(statbuf->st_atime);
	ri.ctime = cpu_to_je32(statbuf->st_ctime);
	ri.mtime = cpu_to_je32(statbuf->st_mtime);
	ri.isize = cpu_to_je32(statbuf->st_size);

	while ((len = read(fd, buf, page_size))) {
		unsigned char *tbuf = buf;

		if (len < 0) {
			sys_errmsg("read");
		}

		if ((printProgress) || (progressBar)) {
			all_read += len;
			if (printProgress)
				printProgressData(false);
			if (progressBar)
				paintProgressBar();
		}

		while (len) {
			uint32_t dsize, space;
			uint16_t compression;

			pad_block_if_less_than(sizeof(ri) + JFFS2_MIN_DATA_LEN);

			dsize = len;
			space =
				erase_block_size - (out_ofs % erase_block_size) -
				sizeof(ri);
			if (space > dsize)
				space = dsize;

			compression = jffs2_compress(tbuf, &cbuf, &dsize, &space);

			ri.compr = compression & 0xff;
			ri.usercompr = (compression >> 8) & 0xff;

			if (ri.compr) {
				wbuf = cbuf;
			} else {
				wbuf = tbuf;
				dsize = space;
			}

			ri.totlen = cpu_to_je32(sizeof(ri) + space);
			ri.hdr_crc = cpu_to_je32(mtd_crc32(0,
							   &ri, sizeof(struct jffs2_unknown_node) - 4));

			ri.version = cpu_to_je32(++ver);
			ri.offset = cpu_to_je32(offset);
			ri.csize = cpu_to_je32(space);
			ri.dsize = cpu_to_je32(dsize);
			ri.node_crc = cpu_to_je32(mtd_crc32(0, &ri, sizeof(ri) - 8));
			ri.data_crc = cpu_to_je32(mtd_crc32(0, wbuf, space));

			full_write(out_fd, &ri, sizeof(ri));
			totcomp += sizeof(ri);
			full_write(out_fd, wbuf, space);
			totcomp += space;
			padword();

			if (tbuf != cbuf) {
				free(cbuf);
				cbuf = NULL;
			}

			tbuf += dsize;
			len -= dsize;
			offset += dsize;

		}
	}
	if (!je32_to_cpu(ri.version)) {
		/* Was empty file */
		pad_block_if_less_than(sizeof(ri));

		ri.version = cpu_to_je32(++ver);
		ri.totlen = cpu_to_je32(sizeof(ri));
		ri.hdr_crc = cpu_to_je32(mtd_crc32(0,
						   &ri, sizeof(struct jffs2_unknown_node) - 4));
		ri.csize = cpu_to_je32(0);
		ri.dsize = cpu_to_je32(0);
		ri.node_crc = cpu_to_je32(mtd_crc32(0, &ri, sizeof(ri) - 8));

		full_write(out_fd, &ri, sizeof(ri));
		padword();
	}
	free(buf);
	close(fd);
	return totcomp;
}

void CMkfsJFFS2::write_symlink(struct filesystem_entry *e)
{
	int len;
	struct stat *statbuf;
	struct jffs2_raw_inode ri;

	statbuf = &(e->sb);
	e->ino = ++ino;
	mkfs_debug_msg("writing symlink '%s'  ino=%lu  parent_ino=%lu",
		       e->name, (unsigned long) e->ino,
		       (unsigned long) e->parent->ino);
	write_dirent(e);

	len = strlen(e->link);
	if (len > JFFS2_MAX_SYMLINK_LEN) {
		errmsg("symlink too large. Truncated to %d chars.",
		       JFFS2_MAX_SYMLINK_LEN);
		len = JFFS2_MAX_SYMLINK_LEN;
	}

	memset(&ri, 0, sizeof(ri));

	ri.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	ri.nodetype = cpu_to_je16(JFFS2_NODETYPE_INODE);
	ri.totlen = cpu_to_je32(sizeof(ri) + len);
	ri.hdr_crc = cpu_to_je32(mtd_crc32(0,
					   &ri, sizeof(struct jffs2_unknown_node) - 4));

	ri.ino = cpu_to_je32(e->ino);
	ri.mode = cpu_to_jemode(statbuf->st_mode);
	ri.uid = cpu_to_je16(statbuf->st_uid);
	ri.gid = cpu_to_je16(statbuf->st_gid);
	ri.atime = cpu_to_je32(statbuf->st_atime);
	ri.ctime = cpu_to_je32(statbuf->st_ctime);
	ri.mtime = cpu_to_je32(statbuf->st_mtime);
	ri.isize = cpu_to_je32(statbuf->st_size);
	ri.version = cpu_to_je32(1);
	ri.csize = cpu_to_je32(len);
	ri.dsize = cpu_to_je32(len);
	ri.node_crc = cpu_to_je32(mtd_crc32(0, &ri, sizeof(ri) - 8));
	ri.data_crc = cpu_to_je32(mtd_crc32(0, e->link, len));

	pad_block_if_less_than(sizeof(ri) + len);
	full_write(out_fd, &ri, sizeof(ri));
	full_write(out_fd, e->link, len);
	padword();
}

void CMkfsJFFS2::write_pipe(struct filesystem_entry *e)
{
	struct stat *statbuf;
	struct jffs2_raw_inode ri;

	statbuf = &(e->sb);
	e->ino = ++ino;
	if (S_ISDIR(statbuf->st_mode)) {
		mkfs_debug_msg("writing dir '%s'  ino=%lu  parent_ino=%lu",
			       e->name, (unsigned long) e->ino,
			       (unsigned long) (e->parent) ? e->parent->ino : 1);
	}
	write_dirent(e);

	memset(&ri, 0, sizeof(ri));

	ri.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	ri.nodetype = cpu_to_je16(JFFS2_NODETYPE_INODE);
	ri.totlen = cpu_to_je32(sizeof(ri));
	ri.hdr_crc = cpu_to_je32(mtd_crc32(0,
					   &ri, sizeof(struct jffs2_unknown_node) - 4));

	ri.ino = cpu_to_je32(e->ino);
	ri.mode = cpu_to_jemode(statbuf->st_mode);
	ri.uid = cpu_to_je16(statbuf->st_uid);
	ri.gid = cpu_to_je16(statbuf->st_gid);
	ri.atime = cpu_to_je32(statbuf->st_atime);
	ri.ctime = cpu_to_je32(statbuf->st_ctime);
	ri.mtime = cpu_to_je32(statbuf->st_mtime);
	ri.isize = cpu_to_je32(0);
	ri.version = cpu_to_je32(1);
	ri.csize = cpu_to_je32(0);
	ri.dsize = cpu_to_je32(0);
	ri.node_crc = cpu_to_je32(mtd_crc32(0, &ri, sizeof(ri) - 8));
	ri.data_crc = cpu_to_je32(0);

	pad_block_if_less_than(sizeof(ri));
	full_write(out_fd, &ri, sizeof(ri));
	padword();
}

void CMkfsJFFS2::write_special_file(struct filesystem_entry *e)
{
	jint16_t kdev;
	struct stat *statbuf;
	struct jffs2_raw_inode ri;

	statbuf = &(e->sb);
	e->ino = ++ino;
	write_dirent(e);

	kdev = cpu_to_je16((major(statbuf->st_rdev) << 8) +
			   minor(statbuf->st_rdev));

	memset(&ri, 0, sizeof(ri));

	ri.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	ri.nodetype = cpu_to_je16(JFFS2_NODETYPE_INODE);
	ri.totlen = cpu_to_je32(sizeof(ri) + sizeof(kdev));
	ri.hdr_crc = cpu_to_je32(mtd_crc32(0,
					   &ri, sizeof(struct jffs2_unknown_node) - 4));

	ri.ino = cpu_to_je32(e->ino);
	ri.mode = cpu_to_jemode(statbuf->st_mode);
	ri.uid = cpu_to_je16(statbuf->st_uid);
	ri.gid = cpu_to_je16(statbuf->st_gid);
	ri.atime = cpu_to_je32(statbuf->st_atime);
	ri.ctime = cpu_to_je32(statbuf->st_ctime);
	ri.mtime = cpu_to_je32(statbuf->st_mtime);
	ri.isize = cpu_to_je32(statbuf->st_size);
	ri.version = cpu_to_je32(1);
	ri.csize = cpu_to_je32(sizeof(kdev));
	ri.dsize = cpu_to_je32(sizeof(kdev));
	ri.node_crc = cpu_to_je32(mtd_crc32(0, &ri, sizeof(ri) - 8));
	ri.data_crc = cpu_to_je32(mtd_crc32(0, &kdev, sizeof(kdev)));

	pad_block_if_less_than(sizeof(ri) + sizeof(kdev));
	full_write(out_fd, &ri, sizeof(ri));
	full_write(out_fd, &kdev, sizeof(kdev));
	padword();
}

void CMkfsJFFS2::write_dirent(struct filesystem_entry *e)
{
	char *name = e->name;
	struct jffs2_raw_dirent rd;
	struct stat *statbuf = &(e->sb);
	static uint32_t version = 0;

	memset(&rd, 0, sizeof(rd));

	rd.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	rd.nodetype = cpu_to_je16(JFFS2_NODETYPE_DIRENT);
	rd.totlen = cpu_to_je32(sizeof(rd) + strlen(name));
	rd.hdr_crc = cpu_to_je32(mtd_crc32(0, &rd,
					   sizeof(struct jffs2_unknown_node) - 4));
	rd.pino = cpu_to_je32((e->parent) ? e->parent->ino : 1);
	rd.version = cpu_to_je32(version++);
	rd.ino = cpu_to_je32(e->ino);
	rd.mctime = cpu_to_je32(statbuf->st_mtime);
	rd.nsize = strlen(name);
	rd.type = IFTODT(statbuf->st_mode);
	//rd.unused[0] = 0;
	//rd.unused[1] = 0;
	rd.node_crc = cpu_to_je32(mtd_crc32(0, &rd, sizeof(rd) - 8));
	rd.name_crc = cpu_to_je32(mtd_crc32(0, name, strlen(name)));

	pad_block_if_less_than(sizeof(rd) + rd.nsize);
	full_write(out_fd, &rd, sizeof(rd));
	full_write(out_fd, name, rd.nsize);
	padword();
}

uint32_t CMkfsJFFS2::find_hardlink(struct filesystem_entry *e)
{
	struct rb_node **n = &hardlinks.rb_node;
	struct rb_node *parent = NULL;

	while (*n) {
		struct filesystem_entry *f;
		parent = *n;
		f = rb_entry(parent, struct filesystem_entry, hardlink_rb);
		if ((f->sb.st_dev < e->sb.st_dev) ||
				(f->sb.st_dev == e->sb.st_dev &&
				 f->sb.st_ino < e->sb.st_ino))
			n = &parent->rb_left;
		else if ((f->sb.st_dev > e->sb.st_dev) ||
				(f->sb.st_dev == e->sb.st_dev &&
				 f->sb.st_ino > e->sb.st_ino)) {
			n = &parent->rb_right;
		} else
			return f->ino;
	}

	rb_link_node(&e->hardlink_rb, parent, n);
	rb_insert_color(&e->hardlink_rb, &hardlinks);
	return 0;
}

void CMkfsJFFS2::full_write(int fd, const void *buf, int len)
{
	char* buf1 = (char*)buf;

	while (len > 0) {
		int ret = write(fd, buf1, len);

		if (ret < 0)
			sys_errmsg("write");

		if (ret == 0)
			sys_errmsg("write returned zero");

		len -= ret;
		buf1 += ret;
		out_ofs += ret;
	}
}

void CMkfsJFFS2::pad_block_if_less_than(int req)
{
	if (add_cleanmarkers) {
		if ((out_ofs % erase_block_size) == 0) {
			full_write(out_fd, &cleanmarker, sizeof(cleanmarker));
			pad(cleanmarker_size - sizeof(cleanmarker));
			padword();
		}
	}
	if ((out_ofs % erase_block_size) + req > erase_block_size) {
		padblock();
	}
	if (add_cleanmarkers) {
		if ((out_ofs % erase_block_size) == 0) {
			full_write(out_fd, &cleanmarker, sizeof(cleanmarker));
			pad(cleanmarker_size - sizeof(cleanmarker));
			padword();
		}
	}
}

void CMkfsJFFS2::padblock(void)
{
	while (out_ofs % erase_block_size) {
		full_write(out_fd, ffbuf, min(sizeof(ffbuf),
					      (size_t)(erase_block_size - (out_ofs % erase_block_size))));
	}
}

void CMkfsJFFS2::pad(int req)
{
	while (req) {
		if (req > (int)sizeof(ffbuf)) {
			full_write(out_fd, ffbuf, sizeof(ffbuf));
			req -= sizeof(ffbuf);
		} else {
			full_write(out_fd, ffbuf, req);
			req = 0;
		}
	}
}

void CMkfsJFFS2::recursive_populate_directory(struct filesystem_entry *dir)
{
	struct filesystem_entry *e;
	e = dir->files;
	while (e) {
		if (e->sb.st_nlink >= 1 && (e->ino = find_hardlink(e)))
			write_dirent(e);
		else {
			switch (e->sb.st_mode & S_IFMT) {
				case S_IFDIR:
					write_pipe(e);
					break;
				case S_IFSOCK:
					write_pipe(e);
					break;
				case S_IFIFO:
					write_pipe(e);
					break;
				case S_IFCHR:
					write_special_file(e);
					break;
				case S_IFBLK:
					write_special_file(e);
					break;
				case S_IFLNK:
					write_symlink(e);
					break;
				case S_IFREG:
					write_regular_file(e);
					break;
				default:
					errmsg("Unknown mode %o for %s", e->sb.st_mode,
					       e->fullname);
					break;
			}

			if ((printProgress) || (progressBar)) {
				switch (e->sb.st_mode & S_IFMT) {
					case S_IFDIR:
					case S_IFSOCK:
					case S_IFIFO:
					case S_IFCHR:
					case S_IFBLK:
					case S_IFLNK:
						all_read += e->sb.st_size;
						break;
					default:
						break;
				}
			}
		}

		if (printProgress)
			printProgressData(false);
		if (progressBar)
			paintProgressBar();
		e = e->next;
	}

	e = dir->files;
	while (e) {
		if (S_ISDIR(e->sb.st_mode)) {
			if (e->files) {
				recursive_populate_directory(e);
			}
		}
		e = e->next;
	}
}

void CMkfsJFFS2::create_target_filesystem(struct filesystem_entry *root)
{
	cleanmarker.magic    = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	cleanmarker.nodetype = cpu_to_je16(JFFS2_NODETYPE_CLEANMARKER);
	cleanmarker.totlen   = cpu_to_je32(cleanmarker_size);
	cleanmarker.hdr_crc  = cpu_to_je32(mtd_crc32(0, &cleanmarker, sizeof(struct jffs2_unknown_node)-4));

	if (ino == 0)
		ino = 1;

	root->ino = 1;
	recursive_populate_directory(root);

	if (pad_fs_size == -1) {
		padblock();
	} else {
		if (pad_fs_size && add_cleanmarkers) {
			padblock();
			while (out_ofs < pad_fs_size) {
				full_write(out_fd, &cleanmarker, sizeof(cleanmarker));
				pad(cleanmarker_size - sizeof(cleanmarker));
				padblock();
			}
		} else {
			while (out_ofs < pad_fs_size) {
				full_write(out_fd, ffbuf, min(sizeof(ffbuf), (size_t)(pad_fs_size - out_ofs)));
			}

		}
	}
}

/*  device table entries take the form of:
	<path>	<type> <mode>	<uid>	<gid>	<major>	<minor>	<start>	<inc>	<count>
	/dev/mem     c    640       0       0	 1       1       0     0	 -

	type can be one of:
	f	A regular file
	d	Directory
	c	Character special device file
	b	Block special device file
	p	Fifo (named pipe)

	I don't bother with symlinks (permissions are irrelevant), hard
	links (special cases of regular files), or sockets (why bother).

	Regular files must exist in the target root directory.  If a char,
	block, fifo, or directory does not exist, it will be created.
 */
bool CMkfsJFFS2::interpret_table_entry(struct filesystem_entry *root, const char *line)
{
	char *hostpath;
	char type;
	char name[512];
	unsigned long mode = 0755, uid = 0, gid = 0, major = 0, minor = 0;
	unsigned long start = 0, increment = 1, count = 0;
	struct filesystem_entry *entry;

	memset(name, '\0', 512);
	if (sscanf(line, "%511s %c %lo %lu %lu %lu %lu %lu %lu %lu",
		   name, &type, &mode, &uid, &gid, &major, &minor, &start, &increment, &count) < 0)
		return false;

	if (!strcmp(name, "/")) {
		sys_errmsg("Device table entries require absolute paths");
	}

	xasprintf(&hostpath, "%s%s", rootdir.c_str(), name);

	/* Check if this file already exists... */
	switch (type) {
		case 'd':
			mode |= S_IFDIR;
			break;
		case 'f':
			mode |= S_IFREG;
			break;
		case 'p':
			mode |= S_IFIFO;
			break;
		case 'c':
			mode |= S_IFCHR;
			break;
		case 'b':
			mode |= S_IFBLK;
			break;
		case 'l':
			mode |= S_IFLNK;
			break;
		default:
			sys_errmsg("Unsupported file type '%c'", type);
	}
	entry = find_filesystem_entry(root, name, mode);
	if (entry && !(count > 0 && (type == 'c' || type == 'b'))) {
		/* Ok, we just need to fixup the existing entry
		 * and we will be all done... */
		entry->sb.st_uid = uid;
		entry->sb.st_gid = gid;
		entry->sb.st_mode = mode;
		if (major && minor) {
			entry->sb.st_rdev = makedev(major, minor);
		}
	} else {
		/* If parent is NULL (happens with device table entries),
		 * try and find our parent now) */
		char *tmp, *dir;
		struct filesystem_entry *parent;
		tmp = strdup(name);
		dir = dirname(tmp);
		parent = find_filesystem_entry(root, dir, S_IFDIR);
		free(tmp);
		if (parent == NULL) {
			errmsg ("skipping device_table entry '%s': no parent directory!", name);
			free(hostpath);
			return false;
		}

		switch (type) {
			case 'd':
				add_host_filesystem_entry(name, hostpath, uid, gid, mode, 0, parent);
				break;
			case 'f':
				add_host_filesystem_entry(name, hostpath, uid, gid, mode, 0, parent);
				break;
			case 'p':
				add_host_filesystem_entry(name, hostpath, uid, gid, mode, 0, parent);
				break;
			case 'c':
			case 'b':
				if (count > 0) {
					dev_t rdev;
					unsigned long i;
					char *dname, *hpath;

					for (i = start; i < (start + count); i++) {
						xasprintf(&dname, "%s%lu", name, i);
						xasprintf(&hpath, "%s/%s%lu", rootdir.c_str(), name, i);
						rdev = makedev(major, minor + (i - start) * increment);
						add_host_filesystem_entry(dname, hpath, uid, gid,
									  mode, rdev, parent);
						free(dname);
						free(hpath);
					}
				} else {
					dev_t rdev = makedev(major, minor);
					add_host_filesystem_entry(name, hostpath, uid, gid,
								  mode, rdev, parent);
				}
				break;
			default:
				sys_errmsg("Unsupported file type '%c'", type);
		}
	}
	free(hostpath);
	return true;
}

bool CMkfsJFFS2::parse_device_table(struct filesystem_entry *root, v_devtable_t *v_devtable)
{
	/* Turn off squash, since we must ensure that values
	 * entered via the device table are not squashed */
	squash_uids  = 0;
	squash_perms = 0;

	bool status = false;
	std::vector<std::string>::iterator it = v_devtable->begin();
	while (it < v_devtable->end()) {
		std::string line = *it;
		line = trim(line);
		if (interpret_table_entry(root, line.c_str()))
			status = true;
		++it;
	}
	return status;
}

struct filesystem_entry *CMkfsJFFS2::recursive_add_host_directory(
				struct filesystem_entry *parent, const char *targetpath,
				const char *hostpath, bool skipSpezialFolders,
				CProgressWindow *progress) {
	int i, n;
	struct stat sb;
	char *hpath, *tpath;
	struct dirent **namelist;
	struct filesystem_entry *entry;
	bool skipCheck = false;

	if (lstat(hostpath, &sb)) {
		sys_errmsg("%s", hostpath);
	}
	if (skipSpezialFolders) {
		// check for spezial folders and mounted folders
		struct statfs s;
		::statfs(hostpath, &s);
		switch (s.f_type) {
			case 0xEF53L:		/*EXT2 & EXT3*/
			case 0x6969L:		/*NFS*/
			case 0xFF534D42L:	/*CIFS*/
			case 0x517BL:		/*SMB*/
			case 0x52654973L:	/*REISERFS*/
			case 0x65735546L:	/*fuse for ntfs*/
			case 0x58465342L:	/*xfs*/
			case 0x4d44L:		/*msdos*/
				skipCheck = true;
				break;
		}

		if (((sb.st_dev == dev_x[dev_dev])  && (strstr(targetpath, "/dev")  == targetpath)) ||
		    ((sb.st_dev == dev_x[dev_proc]) && (strstr(targetpath, "/proc") == targetpath)) ||
		    ((sb.st_dev == dev_x[dev_sys])  && (strstr(targetpath, "/sys")  == targetpath)) ||
		    ((sb.st_dev == dev_x[dev_tmp])  && (strstr(targetpath, "/tmp")  == targetpath))) {
			skipCheck = true;
		}

		if ((!skipCheck) && (sb.st_dev != dev_x[dev_jffs2]))		/* jffs2 */
			return NULL;
	}

	entry = add_host_filesystem_entry(targetpath, hostpath,
					  sb.st_uid, sb.st_gid, sb.st_mode, 0, parent);

	if ((skipSpezialFolders) && (skipCheck))
		return NULL;

#if 0
	if (strstr(targetpath, "/var/epg") == targetpath)
		return NULL;
#endif

	n = scandir(hostpath, &namelist, 0, alphasort);
	if (n < 0) {
		sys_errmsg("opening directory %s", hostpath);
	}

	int i_pr = 0, n_pr = 0;
	if (progress != NULL) {
		n_pr = (n <= 2) ? 1 : n-2;
	}
	for (i=0; i<n; i++) {
		struct dirent *dp = namelist[i];
		if (dp->d_name[0] == '.' && (dp->d_name[1] == 0 ||
	           (dp->d_name[1] == '.' &&  dp->d_name[2] == 0))) {
			free(dp);
			continue;
		}

		xasprintf(&hpath, "%s/%s", hostpath, dp->d_name);
		if (lstat(hpath, &sb)) {
			sys_errmsg("%s", hpath);
		}
		if (strcmp(targetpath, "/") == 0) {
			xasprintf(&tpath, "%s%s", targetpath, dp->d_name);
		} else {
			xasprintf(&tpath, "%s/%s", targetpath, dp->d_name);
		}

		switch (sb.st_mode & S_IFMT) {
			case S_IFDIR:
				recursive_add_host_directory(entry, tpath, hpath, skipSpezialFolders);
				break;

			case S_IFREG:
			case S_IFSOCK:
			case S_IFIFO:
			case S_IFLNK:
			case S_IFCHR:
			case S_IFBLK:
				add_host_filesystem_entry(tpath, hpath, sb.st_uid,
							  sb.st_gid, sb.st_mode, sb.st_rdev, entry);
				break;

			default:
				errmsg("Unknown file type %o for %s", sb.st_mode, hpath);
				break;
		}
		free(dp);
		free(hpath);
		free(tpath);
		if (progress != NULL) {
			progress->showLocalStatus((i_pr * 100) / n_pr);
			i_pr++;
		}
	}
	free(namelist);
	return (entry);
}

#ifdef __GNUC__
#define GETCWD_SIZE 0
#else
#define GETCWD_SIZE -1
#endif

bool CMkfsJFFS2::makeJffs2Image(std::string& path,
				std::string& imageName,
				int eraseBlockSize,
				int padFsSize,
				int addCleanmarkers,
				int targetEndian,
				bool skipSpezialFolders,
				bool useSumtool,
				CProgressWindow *progress/*=NULL*/,
				v_devtable_t *v_devtable/*=NULL*/)
{

	Init();

	imageName_		= imageName;
	useSumtool_		= useSumtool;
	erase_block_size	= eraseBlockSize;	// -e
	pad_fs_size		= padFsSize;		// -p
	add_cleanmarkers	= addCleanmarkers;	// -n
	target_endian		= targetEndian;		// -l
	squash_uids		= 1;			// -U
	rootdir			= path;			// -r
	printProgress		= 1;
	progressBar		= progress;
	hardlinks.rb_node	= NULL;

	printf("[%s] erase_block_size: 0x%X\n", __FUNCTION__, eraseBlockSize);
	classInit();

	char *cwd;
	struct stat sb;
	if (lstat(rootdir.c_str(), &sb)) {
		sys_errmsg("%s", rootdir.c_str());
	}
	if (chdir(rootdir.c_str()))
		sys_errmsg("%s", rootdir.c_str());
	if (!(cwd = getcwd(0, GETCWD_SIZE)))
		sys_errmsg("getcwd failed");

	if (skipSpezialFolders) {
		std::string tmpPath = (path == "/") ? "" : path;
		std::string path_x[dev_max] = {tmpPath + "/sys", tmpPath + "/proc", tmpPath + "/tmp", tmpPath + "/dev", tmpPath + "/"};

		for (int ix = dev_sys; ix < dev_max; ix++) {
			if (lstat(path_x[ix].c_str(), &sb) == 0)
				dev_x[ix] = sb.st_dev;
			else
				dev_x[ix] = 0;
		}
	}

	if (progressBar != NULL) {
		progressBar->showLocalStatus(0);
		progressBar->showGlobalStatus(0);
		progressBar->showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_MKFS_PREPARING_FILES));
	}
	fse_root = recursive_add_host_directory(NULL, "/", cwd, skipSpezialFolders, progressBar);
	if (progressBar != NULL) {
		progressBar->showLocalStatus(100);
		progressBar->showGlobalStatus(50);
	}

	if ((v_devtable != NULL) && (!v_devtable->empty()))
		parse_device_table(fse_root, v_devtable);

	pid_t pid;
	std::string pcmd = "du -sx " + rootdir;
	FILE* fp = my_popen(pid, pcmd.c_str(), "r");
	if (fp != NULL) {
		char buff[256];
		fgets(buff, sizeof(buff), fp);
		fclose(fp);
		kbUsed = atol(buff);
	}
	if (kbUsed == 0)
		kbUsed = 60000;

	if (progressBar != NULL) {
		progressBar->showLocalStatus(0);
		progressBar->showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_MKFS_CREATE_IMAGE));
	}
	create_target_filesystem(fse_root);
	if (progressBar != NULL) {
		progressBar->showGlobalStatus(90);
		progressBar->showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_MKFS_USING_SUMTOOL));
	}
	if (printProgress)
		printProgressData(true);

	classClear();

	sync();
	if (useSumtool_) {
		CSumtoolJFFS2 st;
		st.sumtool(imageName_, sumName_, eraseBlockSize, ((padFsSize==0)?0:1), addCleanmarkers, targetEndian);
		unlink(imageName_.c_str());
	}
	if (progressBar != NULL) {
		paintProgressBar(true);
		progressBar->showGlobalStatus(100);
		sync();
		sleep(2);
	}

	progressBar = NULL;
	return true;
}
