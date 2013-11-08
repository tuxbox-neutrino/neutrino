
#ifndef __MKFS_JFFS2__
#define __MKFS_JFFS2__

/*
	Neutrino-HD

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#include <string>
#include <vector>

#include <gui/widget/progresswindow.h>
#include <system/helpers.h>

class CMkfsJFFS2
{
	public:
		typedef std::vector<std::string> v_devtable_t;

	private:
		enum {
			dev_sys		= 0,
			dev_proc	= 1,
			dev_tmp		= 2,
			dev_dev		= 3,
			dev_jffs2	= 4,
			dev_max		= 5
		};

		int out_fd;
		std::string rootdir;
		int squash_uids;
		int squash_perms;
		int fake_times;
		unsigned long int all_read;
		int printProgress;
		long kbUsed;
		dev_t dev_x[dev_max];
		CProgressWindow *progressBar;
		std::string imageName_, sumName_;
		bool useSumtool_;

		uint32_t ino;
		int out_ofs;
		int erase_block_size;
		int pad_fs_size;
		int add_cleanmarkers;
		int cleanmarker_size;
		unsigned char ffbuf[16];
		bool compressorIsInit;

		void Init();
		void classClear();
		bool classInit();
		struct filesystem_entry *recursive_add_host_directory(
						struct filesystem_entry *parent, const char *targetpath,
						const char *hostpath, bool skipSpezialFolders,
						CProgressWindow *progress=NULL);
		bool parse_device_table(struct filesystem_entry *root, v_devtable_t *v_devtable);
		bool interpret_table_entry(struct filesystem_entry *root, const char *line);
		void create_target_filesystem(struct filesystem_entry *root);
		void recursive_populate_directory(struct filesystem_entry *dir);
		void padblock(void);
		void pad(int req);
		inline void padword(void) { if (out_ofs % 4) full_write(out_fd, ffbuf, 4 - (out_ofs % 4)); }
		inline void pad_block_if_less_than(int req);
		void full_write(int fd, const void *buf, int len);
		static uint32_t find_hardlink(struct filesystem_entry *e);
		void write_dirent(struct filesystem_entry *e);
		void write_special_file(struct filesystem_entry *e);
		void write_pipe(struct filesystem_entry *e);
		void write_symlink(struct filesystem_entry *e);
		unsigned int write_regular_file(struct filesystem_entry *e);
		void paintProgressBar(bool finish=false);
		void printProgressData(bool finish=false);
		struct filesystem_entry *find_filesystem_entry(struct filesystem_entry *dir, char *fullname, uint32_t type);
		struct filesystem_entry *add_host_filesystem_entry(const char *name,
							const char *path, unsigned long uid, unsigned long gid,
							unsigned long mode, dev_t rdev, struct filesystem_entry *parent);
		void cleanup(struct filesystem_entry *dir);
		char *xreadlink(const char *path);

	public:
		CMkfsJFFS2();
		~CMkfsJFFS2();

		bool makeJffs2Image(std::string& path,
				    std::string& imageName,
				    int eraseBlockSize,
				    int padFsSize,
				    int addCleanmarkers,
				    int targetEndian,
				    bool skipSpezialFolders,
				    bool useSumtool,
				    CProgressWindow *progress=NULL,
				    v_devtable_t *v_devtable=NULL);

};

#endif // __MKFS_JFFS2__
