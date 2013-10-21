
#ifndef __SUMTOOL_JFFS2__
#define __SUMTOOL_JFFS2__

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

class CSumtoolJFFS2
{
	private:
		int verbose;
		int padto;
		int add_cleanmarkers;
		int use_input_cleanmarker_size;
		int found_cleanmarkers;
		int cleanmarker_size;
		int erase_block_size;
		int out_fd;
		int in_fd;
		uint8_t *data_buffer;
		unsigned int data_ofs;
		uint8_t *file_buffer;
		unsigned int file_ofs;
		unsigned char ffbuf[16];
		std::string imageName_, sumName_;

		void Init();
		void classClear();
		bool classInit();
		void clean_buffers(void);
		static void init_sumlist(void);
		void init_buffers(void);
		int load_next_block(void);
		void create_summed_image(int inp_size);
		void flush_buffers(void);
		static void clean_sumlist(void);
		void dump_sum_records(void);
		void write_buff_to_file(void);
		void full_write(void *target_buff, const void *buf, int len);
		void pad(int req);
		inline void padword(void) { if (data_ofs % 4) full_write(data_buffer + data_ofs, ffbuf, 4 - (data_ofs % 4)); }
		inline void pad_block_if_less_than(int req,int plus);
		void write_xref_to_buff(union jffs2_node_union *node);
		void write_xattr_to_buff(union jffs2_node_union *node);
		void write_inode_to_buff(union jffs2_node_union *node);
		void write_dirent_to_buff(union jffs2_node_union *node);
		static int add_sum_mem(union jffs2_sum_mem *item);
		void add_sum_xref_mem(union jffs2_node_union *node);
		void add_sum_xattr_mem(union jffs2_node_union *node);
		void add_sum_dirent_mem(union jffs2_node_union *node);
		void add_sum_inode_mem(union jffs2_node_union *node);
		void setup_cleanmarker(void);

	public:
		CSumtoolJFFS2();
		~CSumtoolJFFS2();


		bool sumtool(std::string& imageName,
			     std::string& sumName,
			     int eraseBlockSize=0x20000,
			     int padTo=0,
			     int addCleanmarkers=0,
			     int targetEndian=__LITTLE_ENDIAN);
};

#endif // __SUMTOOL_JFFS2__
