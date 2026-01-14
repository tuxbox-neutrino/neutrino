/*
	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.

	Originally based on the VDR plugin below, heavily modified for Neutrino.
*/

/*
 * radioaudio.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * This is a "plugin" for the Video Disk Recorder (VDR).
 *
 * Written by:                  Lars Tegeler <email@host.dom>
 *
 * Project's homepage:          www.math.uni-paderborn.de/~tegeler/vdr
 *
 * Latest version available at: URL
 *
 * See the file COPYING for license information.
 *
 * Note: The Neutrino version diverged over time and now includes
 * LATM/UECP parsing and extended radiotext handling.
 *
 * Description:
 *
 * This Plugin display an background image while the vdr is switcht to radio channels.
 *

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <malloc.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <vector>
#include <config.h>
#include <system/debug.h>
#include <global.h>
#include <system/settings.h>
#include <system/set_threadname.h>
#include <neutrino.h>
#include <gui/color.h>

#include "radiotext.h"
#include "radiotools.h"
#include <gui/radiotext_window.h>

rtp_classes rtp_content;

namespace
{

/**
 * LATM/RDS decode path:
 * - PES audio payload -> LATM sync (0x56, 0xe0 mask)
 * - parse StreamMuxConfig (audio_mux_version_A == 0)
 * - read LATM payload length and scan for DSE blocks
 * - UECP frames (0xfe..0xff, 0xfd escaping) carry RDS MEC messages (RT/RT+)
 *
 * Debugging:
 * - RADIOTEXT_VERBOSE=0..3 prints UECP/MEC details
 * - RADIOTEXT_DUMP=1 or a path writes hex lines to /tmp/radiotext_dse.log
 * - RADIOTEXT_DUMP_MAX limits output size (default 524288)
 * - DSE = strict match, DSE_SCAN = heuristic scan, OTHERDATA = other_data bytes
 * - OTHERDATA dump is limited to 512 bytes, remaining bits are skipped
 * - UECP frames reset after 5s without an end marker to avoid long stalls
 */
struct LatmConfig
{
	bool valid;
	int audio_mux_version_A;
	int frame_length_type;
	int frame_length;

	LatmConfig()
		: valid(false)
		, audio_mux_version_A(0)
		, frame_length_type(0)
		, frame_length(0)
	{
	}
};

static LatmConfig latm_cfg;
static std::vector<unsigned char> latm_pending;
static const size_t latm_pending_max = 8192;
static int latm_dse_hits = 0;
static int latm_uecp_ok = 0;
static int latm_uecp_crc_fail = 0;
static FILE *latm_dump_fp = NULL;
static unsigned int latm_dump_bytes = 0;
static unsigned int latm_dump_limit = 0;
static bool latm_dump_enabled = false;
static char latm_dump_path[256];
static const unsigned int latm_dump_default_limit = 512 * 1024;
static const unsigned int latm_other_dump_max = 512;
static const unsigned int latm_uecp_timeout_sec = 5;

static void latm_dump_close()
{
	if (latm_dump_fp)
	{
		fclose(latm_dump_fp);
		latm_dump_fp = NULL;
	}
	latm_dump_enabled = false;
	latm_dump_bytes = 0;
	latm_dump_limit = 0;
	latm_dump_path[0] = '\0';
}

static void latm_dump_setup(uint pid)
{
	const char *dump_env = getenv("RADIOTEXT_DUMP");
	if (!dump_env || !*dump_env || !strcmp(dump_env, "0"))
	{
		latm_dump_close();
		return;
	}

	const char *path = dump_env;
	if (!strcmp(dump_env, "1"))
		path = "/tmp/radiotext_dse.log";

	bool reopen = (!latm_dump_fp || strcmp(latm_dump_path, path) != 0);
	if (reopen)
	{
		latm_dump_close();
		latm_dump_fp = fopen(path, "a");
		if (!latm_dump_fp)
			return;
		strncpy(latm_dump_path, path, sizeof(latm_dump_path) - 1);
		latm_dump_path[sizeof(latm_dump_path) - 1] = '\0';
		setvbuf(latm_dump_fp, NULL, _IOLBF, 0);
		latm_dump_bytes = 0;
	}

	latm_dump_enabled = true;
	latm_dump_limit = latm_dump_default_limit;
	const char *limit_env = getenv("RADIOTEXT_DUMP_MAX");
	if (limit_env && *limit_env)
	{
		unsigned long limit = strtoul(limit_env, NULL, 0);
		if (limit > 0 && limit < 0x7fffffff)
			latm_dump_limit = (unsigned int)limit;
	}

	if (latm_dump_fp)
	{
		time_t now = time(NULL);
		int wrote = fprintf(latm_dump_fp, "SESSION ts=%ld pid=0x%04x\n", (long)now, pid);
		if (wrote > 0)
			latm_dump_bytes += (unsigned int)wrote;
	}
}

static void latm_dump_write_line(const char *tag, const unsigned char *data, int len, uint pid)
{
	if (!latm_dump_enabled || !latm_dump_fp || !data || len <= 0 || !tag || !*tag)
		return;
	if (latm_dump_limit && latm_dump_bytes >= latm_dump_limit)
		return;

	char line[2048];
	time_t now = time(NULL);
	int pos = snprintf(line, sizeof(line), "%s pid=0x%04x len=%d ts=%ld:", tag, pid, len, (long)now);
	if (pos < 0 || pos >= (int)sizeof(line))
		return;
	for (int i = 0; i < len && pos + 3 < (int)sizeof(line); i++)
	{
		pos += snprintf(line + pos, sizeof(line) - pos, " %02x", data[i]);
	}
	if (pos + 1 < (int)sizeof(line))
	{
		line[pos++] = '\n';
		line[pos] = '\0';
	}
	else
	{
		line[sizeof(line) - 1] = '\0';
	}

	if (latm_dump_limit && latm_dump_bytes + (unsigned int)pos > latm_dump_limit)
	{
		latm_dump_enabled = false;
		return;
	}

	if (fwrite(line, 1, pos, latm_dump_fp) != (size_t)pos)
	{
		latm_dump_enabled = false;
		return;
	}
	latm_dump_bytes += (unsigned int)pos;
}

static void latm_dump_write_dse(const unsigned char *data, int len, uint pid)
{
	latm_dump_write_line("DSE", data, len, pid);
}

static void latm_dump_write_dse_scan(const unsigned char *data, int len, uint pid)
{
	latm_dump_write_line("DSE_SCAN", data, len, pid);
}

static void latm_dump_write_other(const unsigned char *data, int len, uint pid)
{
	latm_dump_write_line("OTHERDATA", data, len, pid);
}

/**
 * Minimal bit reader for LATM payloads (MSB-first).
 */
class LatmBitReader
{
	public:
		LatmBitReader(const unsigned char *buf, int len, int start_bit = 0)
			: data(buf)
			, bitpos(start_bit)
			, bitlen(len * 8)
		{
		}

		int bitsLeft() const
		{
			return bitlen - bitpos;
		}

		int getBits(int n)
		{
			if (n <= 0 || bitpos + n > bitlen)
				return -1;
			int val = 0;
			for (int i = 0; i < n; i++)
			{
				unsigned char byte = data[bitpos >> 3];
				int shift = 7 - (bitpos & 7);
				val = (val << 1) | ((byte >> shift) & 0x01);
				bitpos++;
			}
			return val;
		}

		bool skipBits(int n)
		{
			if (n < 0 || bitpos + n > bitlen)
				return false;
			bitpos += n;
			return true;
		}

		void byteAlign()
		{
			int mod = bitpos & 7;
			if (mod)
				bitpos += 8 - mod;
		}

		bool readBytes(unsigned char *out, int count)
		{
			if (count < 0 || bitpos + count * 8 > bitlen)
				return false;
			for (int i = 0; i < count; i++)
			{
				int b = getBits(8);
				if (b < 0)
					return false;
				out[i] = (unsigned char)b;
			}
			return true;
		}

	private:
		const unsigned char *data;
		int bitpos;
		int bitlen;
};

/**
 * Decode LATM variable-length value (length code * 8 bits).
 */
static unsigned int latm_get_value(LatmBitReader &br, bool &ok)
{
	int length = br.getBits(2);
	if (length < 0)
	{
		ok = false;
		return 0;
	}
	int bits = (length + 1) * 8;
	unsigned int value = 0;
	for (int i = 0; i < bits; i++)
	{
		int bit = br.getBits(1);
		if (bit < 0)
		{
			ok = false;
			return 0;
		}
		value = (value << 1) | (bit & 1);
	}
	ok = true;
	return value;
}

/**
 * Read "other_data" bits from StreamMuxConfig and optionally dump them.
 */
static bool latm_read_other_data(LatmBitReader &br, unsigned int other_bits, uint pid)
{
	if (other_bits == 0)
		return true;
	if (other_bits > (unsigned int)br.bitsLeft())
		return false;

	unsigned int dump_bits = other_bits;
	unsigned int max_bits = latm_other_dump_max * 8;
	if (dump_bits > max_bits)
		dump_bits = max_bits;
	unsigned int dump_bytes = (dump_bits + 7) / 8;
	std::vector<unsigned char> buf;
	if (dump_bytes)
		buf.assign(dump_bytes, 0);

	for (unsigned int i = 0; i < other_bits; i++)
	{
		int bit = br.getBits(1);
		if (bit < 0)
			return false;
		if (i < dump_bits)
		{
			unsigned int byte_index = i >> 3;
			unsigned int bit_index = 7 - (i & 7);
			buf[byte_index] |= (bit & 1) << bit_index;
		}
	}

	if (dump_bytes)
		latm_dump_write_other(buf.data(), dump_bytes, pid);
	return true;
}

/**
 * Skip AudioSpecificConfig to reach frame length fields.
 */
static bool latm_skip_audio_specific_config(LatmBitReader &br)
{
	int aot = br.getBits(5);
	if (aot < 0)
		return false;
	if (aot == 31)
	{
		int aot_ext = br.getBits(6);
		if (aot_ext < 0)
			return false;
		aot = 32 + aot_ext;
	}
	int sf_index = br.getBits(4);
	if (sf_index < 0)
		return false;
	if (sf_index == 0x0f)
	{
		if (!br.skipBits(24))
			return false;
	}
	if (br.getBits(4) < 0)
		return false;
	if (aot == 5 || aot == 29)
	{
		int sf_ext = br.getBits(4);
		if (sf_ext < 0)
			return false;
		if (sf_ext == 0x0f)
		{
			if (!br.skipBits(24))
				return false;
		}
		int aot2 = br.getBits(5);
		if (aot2 < 0)
			return false;
		if (aot2 == 31)
		{
			int aot_ext = br.getBits(6);
			if (aot_ext < 0)
				return false;
			aot2 = 32 + aot_ext;
		}
		if (br.getBits(4) < 0)
			return false;
	}
	return true;
}

/**
 * Parse LATM StreamMuxConfig. Only audio_mux_version_A == 0 is supported.
 */
static bool latm_read_stream_mux_config(LatmBitReader &br, LatmConfig &cfg, uint pid)
{
	int audio_mux_version = br.getBits(1);
	if (audio_mux_version < 0)
		return false;
	cfg.audio_mux_version_A = 0;
	if (audio_mux_version)
	{
		cfg.audio_mux_version_A = br.getBits(1);
		if (cfg.audio_mux_version_A < 0)
			return false;
	}

	if (!cfg.audio_mux_version_A)
	{
		if (audio_mux_version)
		{
			bool ok = false;
			latm_get_value(br, ok); // taraFullness
			if (!ok)
				return false;
		}

		if (br.getBits(1) < 0) // allStreamsSameTimeFraming
			return false;
		if (br.getBits(6) < 0) // numSubFrames
			return false;
		if (br.getBits(4) != 0) // numPrograms
			return false;
		if (br.getBits(3) != 0) // numLayer
			return false;

		if (!audio_mux_version)
		{
			if (!latm_skip_audio_specific_config(br))
				return false;
		}
		else
		{
			bool ok = false;
			unsigned int asc_len = latm_get_value(br, ok);
			if (!ok || !br.skipBits((int)asc_len))
				return false;
		}

		cfg.frame_length_type = br.getBits(3);
		if (cfg.frame_length_type < 0)
			return false;
		switch (cfg.frame_length_type)
		{
			case 0:
				if (br.getBits(8) < 0) // latmBufferFullness
					return false;
				break;
			case 1:
				cfg.frame_length = br.getBits(9);
				if (cfg.frame_length < 0)
					return false;
				break;
			case 3:
			case 4:
			case 5:
				if (br.getBits(6) < 0)
					return false;
				break;
			case 6:
			case 7:
				if (br.getBits(1) < 0)
					return false;
				break;
		}

		int other_data = br.getBits(1);
		if (other_data < 0)
			return false;
		if (other_data)
		{
			if (audio_mux_version)
			{
				bool ok = false;
				unsigned int other_bits = latm_get_value(br, ok);
				if (!ok)
					return false;
				if (!latm_read_other_data(br, other_bits, pid))
					return false;
			}
			else
			{
				int esc;
				std::vector<unsigned char> buf;
				do
				{
					esc = br.getBits(1);
					int tmp = br.getBits(8);
					if (esc < 0 || tmp < 0)
						return false;
					if (buf.size() < latm_other_dump_max)
						buf.push_back((unsigned char)tmp);
				}
				while (esc);
				if (!buf.empty())
					latm_dump_write_other(buf.data(), (int)buf.size(), pid);
			}
		}

		int crc_present = br.getBits(1);
		if (crc_present < 0)
			return false;
		if (crc_present && br.getBits(8) < 0)
			return false;
	}

	cfg.valid = true;
	return true;
}

/**
 * Read payload length info according to frame_length_type.
 */
static int latm_read_payload_length_info(LatmBitReader &br, const LatmConfig &cfg)
{
	if (cfg.frame_length_type == 0)
	{
		int mux_slot_length = 0;
		int tmp;
		do
		{
			tmp = br.getBits(8);
			if (tmp < 0)
				return -1;
			mux_slot_length += tmp;
		}
		while (tmp == 255);
		return mux_slot_length;
	}
	if (cfg.frame_length_type == 1)
		return cfg.frame_length;
	if (cfg.frame_length_type == 3 || cfg.frame_length_type == 5 || cfg.frame_length_type == 7)
	{
		if (br.getBits(2) < 0)
			return -1;
	}
	return 0;
}

/**
 * Validate that the remaining AudioMuxElement only contains FILL/END.
 */
static bool latm_tail_is_end_or_fill(LatmBitReader &br)
{
	while (br.bitsLeft() >= 3)
	{
		int elem_type = br.getBits(3);
		if (elem_type < 0)
			return false;
		if (elem_type == 7)
		{
			while (br.bitsLeft() > 0)
			{
				int bit = br.getBits(1);
				if (bit < 0 || bit != 0)
					return false;
			}
			return true;
		}
		if (elem_type != 6)
			return false;
		int count = br.getBits(4);
		if (count < 0)
			return false;
		if (count == 15)
		{
			int esc = br.getBits(8);
			if (esc < 0)
				return false;
			count += esc - 1;
		}
		if (count < 0)
			return false;
		if (!br.skipBits(count * 8))
			return false;
	}
	return false;
}

/**
 * Check for UECP start byte (0xfe) inside a buffer.
 */
static bool latm_buffer_has_uecp_start(const unsigned char *data, int len)
{
	if (!data || len <= 0)
		return false;
	for (int i = 0; i < len; i++)
	{
		if (data[i] == 0xfe)
			return true;
	}
	return false;
}

/**
 * Strict DSE search: element type 4 followed by END/FILL tail.
 */
static bool latm_find_strict_dse(const unsigned char *data, int len, std::vector<unsigned char> &out)
{
	const int total_bits = len * 8;
	for (int bit = 0; bit + 16 < total_bits; bit++)
	{
		LatmBitReader br(data, len, bit);
		int elem_type = br.getBits(3);
		if (elem_type != 4)
			continue;
		if (br.getBits(4) < 0)
			continue;
		int align = br.getBits(1);
		if (align < 0)
			continue;
		int count = br.getBits(8);
		if (count < 0)
			continue;
		if (count == 255)
		{
			int esc = br.getBits(8);
			if (esc < 0)
				continue;
			count += esc;
		}
		if (count <= 0 || count > 512)
			continue;
		if (align)
			br.byteAlign();
		if (br.bitsLeft() < count * 8 + 3)
			continue;
		std::vector<unsigned char> buf(count);
		if (!br.readBytes(buf.data(), count))
			continue;
		if (!latm_tail_is_end_or_fill(br))
			continue;
		out.swap(buf);
		return true;
	}
	return false;
}

} // namespace

/**
 * Find DSE blocks in a LATM payload and feed UECP frames.
 * Strict DSE requires an END/FILL tail; heuristic scan is used as fallback.
 * UECP frames are only parsed if a start marker (0xfe) is present.
 */
bool CRadioText::latm_scan_dse(const unsigned char *data, int len)
{
	std::vector<unsigned char> strict;
	bool found_dse = false;
	if (latm_find_strict_dse(data, len, strict))
	{
		latm_dse_hits++;
		bool strict_candidate = uecp_in_frame;
		if (!strict.empty())
		{
			latm_dump_write_dse(strict.data(), (int)strict.size(), pid);
			if (!strict_candidate)
				strict_candidate = latm_buffer_has_uecp_start(strict.data(), (int)strict.size());
		}
		if (strict_candidate && !strict.empty())
		{
			found_dse = true;
			if (processUecpBuffer(strict.data(), (int)strict.size()))
			{
				latm_uecp_ok++;
				return true;
			}
			return true;
		}
	}

	const int total_bits = len * 8;
	for (int bit = 0; bit + 16 < total_bits; bit += 8)
	{
		LatmBitReader br(data, len, bit);
		int elem_type = br.getBits(3);
		if (elem_type != 4)
			continue;
		if (br.getBits(4) < 0)
			continue;
		int align = br.getBits(1);
		if (align < 0)
			continue;
		int count = br.getBits(8);
		if (count < 0)
			continue;
		if (count == 255)
		{
			int esc = br.getBits(8);
			if (esc < 0)
				continue;
			count += esc;
		}
		if (count <= 0 || count > 512)
			continue;
		latm_dse_hits++;
		found_dse = true;
		if (align)
			br.byteAlign();
		std::vector<unsigned char> buf(count);
		if (!br.readBytes(buf.data(), count))
			continue;
		if (buf.empty())
			continue;
		latm_dump_write_dse_scan(buf.data(), count, pid);
		bool should_feed = uecp_in_frame;
		if (!should_feed)
			should_feed = latm_buffer_has_uecp_start(buf.data(), count);
		if (!should_feed)
			continue;
		if (processUecpBuffer(buf.data(), count))
		{
			latm_uecp_ok++;
			return true;
		}
	}
	return found_dse;
}

/**
 * Parse a LATM AudioMuxElement and extract its payload for UECP/DSE parsing.
 */
bool CRadioText::latm_process_frame(const unsigned char *data, int len)
{
	LatmBitReader br(data, len);
	int use_same_mux = br.getBits(1);
	if (use_same_mux < 0)
		return false;
	if (!use_same_mux)
	{
		if (!latm_read_stream_mux_config(br, latm_cfg, pid))
			return false;
	}
	if (!latm_cfg.valid || latm_cfg.audio_mux_version_A != 0)
		return false;

	int payload_len = latm_read_payload_length_info(br, latm_cfg);
	if (payload_len <= 0 || br.bitsLeft() < payload_len * 8)
		return false;

	std::vector<unsigned char> payload(payload_len);
	if (!br.readBytes(payload.data(), payload_len))
		return false;

	if (latm_scan_dse(payload.data(), payload_len))
		return true;

	return processUecpBuffer(payload.data(), payload_len);
}

// RDS rest
bool RDS_PSShow = false;
int RDS_PSIndex = 0;
char RDS_PSText[12][9];

// plugin audiorecorder service
bool ARec_Receive = false, ARec_Record = false;

#define floor
const char *DataDir = "./";

// RDS-Chartranslation: 0x80..0xff
unsigned char rds_addchar[128] =
{
	0xe1, 0xe0, 0xe9, 0xe8, 0xed, 0xec, 0xf3, 0xf2,
	0xfa, 0xf9, 0xd1, 0xc7, 0x8c, 0xdf, 0x8e, 0x8f,
	0xe2, 0xe4, 0xea, 0xeb, 0xee, 0xef, 0xf4, 0xf6,
	0xfb, 0xfc, 0xf1, 0xe7, 0x9c, 0x9d, 0x9e, 0x9f,
	0xaa, 0xa1, 0xa9, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xa3, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xba, 0xb9, 0xb2, 0xb3, 0xb1, 0xa1, 0xb6, 0xb7,
	0xb5, 0xbf, 0xf7, 0xb0, 0xbc, 0xbd, 0xbe, 0xa7,
	0xc1, 0xc0, 0xc9, 0xc8, 0xcd, 0xcc, 0xd3, 0xd2,
	0xda, 0xd9, 0xca, 0xcb, 0xcc, 0xcd, 0xd0, 0xcf,
	0xc2, 0xc4, 0xca, 0xcb, 0xce, 0xcf, 0xd4, 0xd6,
	0xdb, 0xdc, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xc3, 0xc5, 0xc6, 0xe3, 0xe4, 0xdd, 0xd5, 0xd8,
	0xde, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xf0,
	0xe3, 0xe5, 0xe6, 0xf3, 0xf4, 0xfd, 0xf5, 0xf8,
	0xfe, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const char *entitystr[5]  = { "&apos;", "&amp;", "&quote;", "&gt", "&lt" };
static const char *entitychar[5] = { "'", "&", "\"", ">", "<" };

char *CRadioText::rds_entitychar(char *text)
{
	int i = 0, l, lof, lre, space;
	char *temp;

	while (i < 5)
	{
		if ((temp = strstr(text, entitystr[i])) != NULL)
		{
			if (S_Verbose >= 2)
				printf("RText-Entity: %s\n", text);
			l = strlen(entitystr[i]);
			lof = (temp - text);
			if (strlen(text) < RT_MEL)
			{
				lre = strlen(text) - lof - l;
				space = 1;
			}
			else
			{
				lre =  RT_MEL - 1 - lof - l;
				space = 0;
			}
			memmove(text + lof, entitychar[i], 1);
			memmove(text + lof + 1, temp + l, lre);
			if (space != 0)
				memmove(text + lof + 1 + lre, "         ", l - 1);
		}
		else
			i++;
	}
	return text;
}

char *CRadioText::ptynr2string(int nr)
{
	switch (nr)
	{
		// Source: http://www.ebu.ch/trev_255-beale.pdf
		case  0: return tr(const_cast<char *>("unknown program type"));
		case  1: return tr(const_cast<char *>("News"));
		case  2: return tr(const_cast<char *>("Current affairs"));
		case  3: return tr(const_cast<char *>("Information"));
		case  4: return tr(const_cast<char *>("Sport"));
		case  5: return tr(const_cast<char *>("Education"));
		case  6: return tr(const_cast<char *>("Drama"));
		case  7: return tr(const_cast<char *>("Culture"));
		case  8: return tr(const_cast<char *>("Science"));
		case  9: return tr(const_cast<char *>("Varied"));
		case 10: return tr(const_cast<char *>("Pop music"));
		case 11: return tr(const_cast<char *>("Rock music"));
		case 12: return tr(const_cast<char *>("M.O.R. music"));
		case 13: return tr(const_cast<char *>("Light classical"));
		case 14: return tr(const_cast<char *>("Serious classical"));
		case 15: return tr(const_cast<char *>("Other music"));
		// 16-30 "Spares"
		case 31: return tr(const_cast<char *>("Alarm"));
		default: return const_cast<char *>("?");
	}
}

/**
 * Locate UECP subpackets inside PES data by 0xff/0xfd delimiters.
 */
bool CRadioText::DividePes(unsigned char *data, int length, int *substart, int *subend)
{
	int i = *substart;
	int found = 0;
	while ((i < length - 2) && (found < 2))
	{
		if ((found == 0) && (data[i] == 0xFF) && (data[i + 1] == 0xFD))
		{
			*substart = i;
			found++;
		}
		else if ((found == 1) && (data[i] == 0xFD) && (data[i + 1] == 0xFF))
		{
			*subend = i;
			found++;
		}
		i++;
	}
	if ((found == 1) && (data[length - 1] == 0xFD))
	{
		*subend = length - 1;
		found++;
	}
	if (found == 2)
	{
		return (true);
	}
	else
	{
		return (false);
	}
}

/**
 * Parse LATM frames from PES audio payloads and feed the LATM decoder.
 */
bool CRadioText::processLatmFromPes(const unsigned char *data, int len)
{
	if (!data || len <= 0)
		return false;

	int start = 0;
	if (len >= 9 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 &&
		(data[3] & 0xE0) == 0xC0)
	{
		int header_len = data[8];
		int payload_start = 9 + header_len;
		if (payload_start >= len)
			return false;
		start = payload_start;
	}

	const unsigned char *payload = data + start;
	int payload_len = len - start;
	bool latm_seen = false;
	bool latm_partial = false;
	int frames = 0;
	size_t keep_from = 0;

	if (payload_len <= 0)
		return false;

	if (latm_pending.size() > latm_pending_max)
		latm_pending.clear();
	latm_pending.insert(latm_pending.end(), payload, payload + payload_len);

	for (size_t i = 0; i + 3 <= latm_pending.size();)
	{
		if (latm_pending[i] == 0x56 && (latm_pending[i + 1] & 0xE0) == 0xE0)
		{
			int frame_len = (((latm_pending[i + 1] & 0x1F) << 8) | latm_pending[i + 2]) + 3;
			if (frame_len > 3)
			{
				if (i + frame_len > latm_pending.size())
				{
					latm_partial = true;
					keep_from = i;
					break;
				}
				latm_seen = true;
				frames++;
				latm_process_frame(&latm_pending[i + 3], frame_len - 3);
				i += frame_len;
				keep_from = i;
				continue;
			}
		}
		i++;
		keep_from = i;
	}

	if (keep_from > 0 && keep_from <= latm_pending.size())
		latm_pending.erase(latm_pending.begin(), latm_pending.begin() + keep_from);

	if (latm_pending.size() > latm_pending_max)
		latm_pending.erase(latm_pending.begin(), latm_pending.end() - latm_pending_max);

	if (S_Verbose >= 2 && (latm_seen || latm_partial))
		printf("RDS-LATM: frames %d dse %d uecp %d crc_fail %d pending %zu bytes\n",
			frames, latm_dse_hits, latm_uecp_ok, latm_uecp_crc_fail, latm_pending.size());

	return latm_seen || latm_cfg.valid;
}

/**
 * Dispatch decoded UECP frames by MEC (RT/RT+/PTY/PS/PTYN).
 */
void CRadioText::handleRdsMessage(unsigned char *mtext, int len)
{
	if (len < 9)
		return;

	int msg_len = len - 1;
	if (msg_len < 0)
		return;

	int mec = mtext[5];
	switch (mec)
	{
		case 0x0a:
			have_radiotext = true;
		/* fall through */
		case 0x46:
			if (S_Verbose >= 2)
				printf("(RDS-MEC '%02x') -> RadiotextDecode - %d\n", mec, msg_len);
			RadiotextDecode(mtext, msg_len);		// Radiotext, RT+
			break;
		case 0x07:
			RT_PTY = mtext[8];			// PTY
			RT_MsgShow = true;
			if (S_Verbose >= 1)
				printf("RDS-PTY set to '%s'\n", ptynr2string(RT_PTY));
			break;
		case 0x3e:
			if (S_Verbose >= 2)
				printf("(RDS-MEC '%02x') -> RDS_PsPtynDecode - %d\n", mec, msg_len);
			RDS_PsPtynDecode(true, mtext, msg_len);	// PTYN
			break;
		case 0x02:
			if (S_Verbose >= 2)
				printf("(RDS-MEC '%02x') -> RDS_PsPtynDecode - %d\n", mec, msg_len);
			RDS_PsPtynDecode(false, mtext, msg_len);	// PS
			break;
		case 0xda:
			break;
	}
}

/**
 * Parse UECP frames from a buffer.
 * UECP uses 0xfe start, 0xff end, 0xfd escaping; CRC is verified.
 * A stuck frame is reset after a short timeout to allow new starts.
 */
bool CRadioText::processUecpBuffer(const unsigned char *data, int len)
{
	if (!data || len <= 0)
		return false;

	bool decoded = false;
	time_t now = time(NULL);
	if (uecp_in_frame && uecp_last_ts > 0 && now > uecp_last_ts)
	{
		if ((unsigned int)(now - uecp_last_ts) > latm_uecp_timeout_sec)
		{
			if (S_Verbose >= 1)
				printf("RDS-UECP: timeout reset after %ld s\n", (long)(now - uecp_last_ts));
			uecp_in_frame = false;
			uecp_escape = false;
			uecp_index = -1;
			uecp_last_ts = 0;
		}
	}

	for (int i = 0; i < len; i++)
	{
		unsigned char val = data[i];
		if (!uecp_in_frame)
		{
			if (val != 0xfe)
				continue;
			uecp_in_frame = true;
			uecp_escape = false;
			uecp_index = -1;
			uecp_buf[++uecp_index] = val;
			uecp_last_ts = now;
			continue;
		}

		if (uecp_escape)
		{
			switch (val)
			{
				case 0x00:
					val = 0xfd;
					break;
				case 0x01:
					val = 0xfe;
					break;
				case 0x02:
					val = 0xff;
					break;
				default:
					break;
			}
			uecp_escape = false;
		}
		else if (val == 0xfd)
		{
			uecp_escape = true;
			continue;
		}

		if (uecp_index + 1 >= (int)sizeof(uecp_buf))
		{
			uecp_in_frame = false;
			uecp_escape = false;
			uecp_index = -1;
			uecp_last_ts = 0;
			continue;
		}

		uecp_buf[++uecp_index] = val;
		uecp_last_ts = now;
		if (val == 0xff)
		{
			if (uecp_index >= 9)
			{
				int frame_len = uecp_index + 1;
				unsigned short tx_crc = (uecp_buf[frame_len - 3] << 8) + uecp_buf[frame_len - 2];
				unsigned short crc16 = crc16_ccitt(uecp_buf, frame_len - 4, true);
				unsigned short crc16_inv = (unsigned short)(~crc16);
				unsigned short crc16_ns = crc16_ccitt(uecp_buf, frame_len - 4, false);
				unsigned short crc16_ns_inv = (unsigned short)(~crc16_ns);
				if (crc16 == tx_crc || crc16_inv == tx_crc || crc16_ns == tx_crc || crc16_ns_inv == tx_crc)
				{
					handleRdsMessage(uecp_buf, frame_len);
					decoded = true;
				}
				else
				{
					latm_uecp_crc_fail++;
				}
			}
			uecp_in_frame = false;
			uecp_escape = false;
			uecp_index = -1;
			uecp_last_ts = 0;
		}
	}

	return decoded;
}

/**
 * Legacy RDS PES decoder (reverse UECP extraction for non-LATM streams).
 */
int CRadioText::PES_Receive(unsigned char *data, int len)
{
	const int mframel = 263;  // max. 255(MSG)+4(ADD/SQC/MFL)+2(CRC)+2(Start/Stop) of RDS-data
	static unsigned char mtext[mframel + 1];
	static bool rt_start = false, rt_bstuff = false;
	static int index;
	int offset = 0;

	while (true)
	{
		if (len < offset + 6)
			return offset;
		int pesl = (data[offset + 4] << 8) + data[offset + 5] + 6;
		if (pesl <= 0 || offset + pesl > len)
			return offset;
		offset += pesl;

		/* try to find subpackets in the pes stream */
		int substart = 0;
		int subend = 0;
		while (DividePes(&data[0], pesl, &substart, &subend))
		{
			int inner_offset = subend + 1;
			if (inner_offset < 3)
				fprintf(stderr, "RT %s: inner_offset < 3 (%d)\n", __FUNCTION__, inner_offset);
			int rdsl = data[subend - 1];	// RDS DataFieldLength
			// RDS DataSync = 0xfd @ end
			if (data[subend] == 0xfd && rdsl > 0)
			{
				// print RawData with RDS-Info
				if (S_Verbose >= 3)
				{
					printf("\n\nPES-Data(%d/%d): ", pesl, len);
					for (int a = inner_offset - rdsl; a < offset; a++)
						printf("%02x ", data[a]);
					printf("(End)\n\n");
				}

				if (subend - 2 - rdsl < 0)
					fprintf(stderr, "RT %s: start: %d subend-2-rdsl < 0 (%d-2-%d)\n", __FUNCTION__, substart, subend, rdsl);
				for (int i = subend - 2, val; i > subend - 2 - rdsl; i--)   // <-- data reverse, from end to start
				{
					if (i < 0)
					{
						fprintf(stderr, "RT %s: i < 0 (%d)\n", __FUNCTION__, i);
						break;
					}
					val = data[i];

					if (val == 0xfe)  	// Start
					{
						index = -1;
						rt_start = true;
						rt_bstuff = false;
						if (S_Verbose >= 2)
							printf("RDS-Start: ");
					}

					if (rt_start)
					{
						if (S_Verbose >= 3)
							printf("%02x ", val);
						// byte-stuffing reverse: 0xfd00->0xfd, 0xfd01->0xfe, 0xfd02->0xff
						if (rt_bstuff)
						{
							switch (val)
							{
								case 0x00: mtext[index] = 0xfd; break;
								case 0x01: mtext[index] = 0xfe; break;
								case 0x02: mtext[index] = 0xff; break;
								default: mtext[++index] = val;	// should never be
							}
							rt_bstuff = false;
							if (S_Verbose >= 3)
								printf("(Bytestuffing -> %02x) ", mtext[index]);
						}
						else
							mtext[++index] = val;
						if (val == 0xfd && index > 0)	// stuffing found
							rt_bstuff = true;
						// early check for used MEC
						if (index == 5)
						{
							//mec = val;
							switch (val)
							{
								case 0x0a:			// RT
									have_radiotext = true;
								/* fall through */
								case 0x46:			// RTplus-Tags
								case 0xda:			// RASS
								case 0x07:			// PTY
								case 0x3e:			// PTYN
								case 0x02:			// PS
									break;
								default:
									rt_start = false;
									if (S_Verbose >= 2)
										printf("(RDS-MEC '%02x' not used -> End)\n", val);
							}
						}
						if (index >= mframel)  		// max. rdslength, garbage ?
						{
							if (S_Verbose >= 1)
								printf("RDS-Error: too long, garbage ?\n");
							rt_start = false;
						}
					}

					if (rt_start && val == 0xff)  	// End
					{
						if (S_Verbose >= 2)
							printf("(RDS-End)\n");
						rt_start = false;
						if (index < 9)  		//  min. rdslength, garbage ?
						{
							if (S_Verbose >= 1)
								printf("RDS-Error: too short -> garbage ?\n");
						}
						else
						{
							// crc16-check
							unsigned short crc16 = crc16_ccitt(mtext, index - 3, true);
							if (crc16 != (mtext[index - 2] << 8) + mtext[index - 1])
							{
								if (S_Verbose >= 1)
									printf("RDS-Error: wrong CRC # calc = %04x <> transmit = %02x%02x\n", crc16, mtext[index - 2], mtext[index - 1]);
							}
							else
							{

								handleRdsMessage(mtext, index + 1);
							}
						}
					}
				}
			}
			substart = subend;
		}
	}
}


/**
 * Decode RT/RT+ elements carried in UECP messages.
 */
void CRadioText::RadiotextDecode(unsigned char *mtext, int len)
{
	static bool rtp_itoggle = false;
	static int rtp_idiffs = 0;
	static cTimeMs rtp_itime;
	static char plustext[RT_MEL];

	// byte 1+2 = ADD (10bit SiteAdress + 6bit EncoderAdress)
	// byte 3   = SQC (Sequence Counter 0x00 = not used)
	int leninfo = mtext[4];	// byte 4 = MFL (Message Field Length)
	if (len >= leninfo + 7)  	// check complete length
	{

		// byte 5 = MEC (Message Element Code, 0x0a for RT, 0x46 for RTplus)
		if (mtext[5] == 0x0a)
		{
			// byte 6+7 = DSN+PSN (DataSetNumber+ProgramServiceNumber,
			//		       	   ignore here, always 0x00 ?)
			// byte 8   = MEL (MessageElementLength, max. 64+1 byte @ RT)
			if (mtext[8] == 0 || mtext[8] > RT_MEL || mtext[8] > leninfo - 4)
			{
				if (S_Verbose >= 1)
					printf("RT-Error: Length = 0 or not correct !");
				return;
			}
			// byte 9 = RT-Status bitcodet (0=AB-flagcontrol, 1-4=Transmission-Number, 5+6=Buffer-Config,
			//				    ingnored, always 0x01 ?)
			fprintf(stderr, "MEC=0x%02x DSN=0x%02x PSN=0x%02x MEL=%02d STATUS=0x%02x MFL=%02d\n", mtext[5], mtext[6], mtext[7], mtext[8], mtext[9], mtext[4]);
			char temptext[RT_MEL];
			memset(temptext, 0x20, RT_MEL - 1);
			temptext[RT_MEL - 1] = '\0';
			for (int i = 1, ii = 0; i < mtext[8]; i++)
			{
				if (mtext[9 + i] <= 0xfe)
					// additional rds-character, see RBDS-Standard, Annex E
					temptext[ii++] = (mtext[9 + i] >= 0x80) ? rds_addchar[mtext[9 + i] - 0x80] : mtext[9 + i];
			}
			memcpy(plustext, temptext, RT_MEL);
			rds_entitychar(temptext);
			// check repeats
			bool repeat = false;
			for (int ind = 0; ind < S_RtOsdRows; ind++)
			{
				if (memcmp(RT_Text[ind], temptext, RT_MEL - 1) == 0)
				{
					repeat = true;
					if (S_Verbose >= 1)
						printf("RText-Rep[%d]: %s\n", ind, RT_Text[ind]);
				}
			}
			if (!repeat)
			{
				memcpy(RT_Text[RT_Index], temptext, RT_MEL);
				// +Memory
				char *temp;
				asprintf(&temp, "%s", RT_Text[RT_Index]);
				if (++rtp_content.rt_Index >= 2 * MAX_RTPC)
					rtp_content.rt_Index = 0;
				asprintf(&rtp_content.radiotext[rtp_content.rt_Index], "%s", rtrim(temp));
				free(temp);
				if (S_Verbose >= 1)
					printf("Radiotext[%d]: %s\n", RT_Index, RT_Text[RT_Index]);
				RT_Index += 1;
				if (RT_Index >= S_RtOsdRows)
					RT_Index = 0;
			}
			RTP_TToggle = 0x03;		// Bit 0/1 = Title/Artist
			RT_MsgShow = true;
			S_RtOsd = 1;
			RT_Info = (RT_Info > 0) ? RT_Info : 1;
			RadioStatusMsg();

			if (!OnAfterDecodeLine.empty())
			{
				if (!OnAfterDecodeLine.blocked())
				{
					dprintf(DEBUG_DEBUG, "\033[36m[CRadioText] %s - %d: signal OnAfterDecodeLine contains %d slot(s)\033[0m\n", __func__, __LINE__, (int)OnAfterDecodeLine.size());
					OnAfterDecodeLine();
				}
				else
				{
					dprintf(DEBUG_DEBUG, "\033[31m[CRadioText] %s - %d: signal OnAfterDecodeLine blocked\033[0m\n", __func__, __LINE__);
				}
			}
		}

		else if (RTP_TToggle > 0 && mtext[5] == 0x46 && S_RtFunc >= 2)  	// RTplus tags V2.0, only if RT
		{
			if (mtext[6] > leninfo - 2 || mtext[6] != 8) // byte 6 = MEL, only 8 byte for 2 tags
			{
				if (S_Verbose >= 1)
					printf("RTp-Error: Length not correct !");
				return;
			}

			uint rtp_typ[2], rtp_start[2], rtp_len[2];
			// byte 7+8 = ApplicationID, always 0x4bd7
			// byte 9   = Applicationgroup Typecode / PTY ?
			// bit 10#4 = Item Togglebit
			// bit 10#3 = Item Runningbit
			// Tag1: bit 10#2..11#5 = Contenttype, 11#4..12#7 = Startmarker, 12#6..12#1 = Length
			rtp_typ[0]   = (0x38 & mtext[10] << 3) | mtext[11] >> 5;
			rtp_start[0] = (0x3e & mtext[11] << 1) | mtext[12] >> 7;
			rtp_len[0]   = 0x3f & mtext[12] >> 1;
			// Tag2: bit 12#0..13#3 = Contenttype, 13#2..14#5 = Startmarker, 14#4..14#0 = Length(5bit)
			rtp_typ[1]   = (0x20 & mtext[12] << 5) | mtext[13] >> 3;
			rtp_start[1] = (0x38 & mtext[13] << 3) | mtext[14] >> 5;
			rtp_len[1]   = 0x1f & mtext[14];
			if (S_Verbose >= 2)
				printf("RTplus (tag=Typ/Start/Len):  Toggle/Run = %d/%d, tag#1 = %d/%d/%d, tag#2 = %d/%d/%d\n",
					(mtext[10] & 0x10) > 0, (mtext[10] & 0x08) > 0, rtp_typ[0], rtp_start[0], rtp_len[0], rtp_typ[1], rtp_start[1], rtp_len[1]);
			// save info
			for (int i = 0; i < 2; i++)
			{
				if (rtp_start[i] + rtp_len[i] + 1 >= RT_MEL)  	// length-error
				{
					if (S_Verbose >= 1)
						printf("RTp-Error (tag#%d = Typ/Start/Len): %d/%d/%d (Start+Length > 'RT-MEL' !)\n",
							i + 1, rtp_typ[i], rtp_start[i], rtp_len[i]);
				}
				else
				{
					char temptext[RT_MEL];
					memset(temptext, 0x20, RT_MEL - 1);
					memmove(temptext, plustext + rtp_start[i], rtp_len[i] + 1);
					rds_entitychar(temptext);
					// +Memory
					memset(rtp_content.temptext, 0x20, RT_MEL - 1);
					memcpy(rtp_content.temptext, temptext, RT_MEL - 1);
					switch (rtp_typ[i])
					{
						case 1:		// Item-Title
							if ((mtext[10] & 0x08) > 0 && (RTP_TToggle & 0x01) == 0x01)
							{
								RTP_TToggle -= 0x01;
								RT_Info = 2;
								if (memcmp(RTP_Title, temptext, RT_MEL - 1) != 0 || (mtext[10] & 0x10) != RTP_ItemToggle)
								{
									memcpy(RTP_Title, temptext, RT_MEL - 1);
									if (RT_PlusShow && rtp_itime.Elapsed() > 1000)
										rtp_idiffs = (int) rtp_itime.Elapsed() / 1000;
									if (!rtp_content.item_New)
									{
										RTP_Starttime = time(NULL);
										rtp_itime.Set(0);
										sprintf(RTP_Artist, "---");
										if (++rtp_content.item_Index >= MAX_RTPC)
											rtp_content.item_Index = 0;
										rtp_content.item_Start[rtp_content.item_Index] = time(NULL);	// todo: replay-mode
										rtp_content.item_Artist[rtp_content.item_Index] = NULL;
									}
									rtp_content.item_New = (!rtp_content.item_New) ? true : false;
									if (rtp_content.item_Index >= 0)
										asprintf(&rtp_content.item_Title[rtp_content.item_Index], "%s", rtrim(rtp_content.temptext));
									RT_PlusShow = RT_MsgShow = rtp_itoggle = true;
								}
							}
							break;
						case 4:		// Item-Artist
							if ((mtext[10] & 0x08) > 0 && (RTP_TToggle & 0x02) == 0x02)
							{
								RTP_TToggle -= 0x02;
								RT_Info = 2;
								if (memcmp(RTP_Artist, temptext, RT_MEL - 1) != 0 || (mtext[10] & 0x10) != RTP_ItemToggle)
								{
									memcpy(RTP_Artist, temptext, RT_MEL - 1);
									if (RT_PlusShow && rtp_itime.Elapsed() > 1000)
										rtp_idiffs = (int) rtp_itime.Elapsed() / 1000;
									if (!rtp_content.item_New)
									{
										RTP_Starttime = time(NULL);
										rtp_itime.Set(0);
										sprintf(RTP_Title, "---");
										if (++rtp_content.item_Index >= MAX_RTPC)
											rtp_content.item_Index = 0;
										rtp_content.item_Start[rtp_content.item_Index] = time(NULL);	// todo: replay-mode
										rtp_content.item_Title[rtp_content.item_Index] = NULL;
									}
									rtp_content.item_New = (!rtp_content.item_New) ? true : false;
									if (rtp_content.item_Index >= 0)
										asprintf(&rtp_content.item_Artist[rtp_content.item_Index], "%s", rtrim(rtp_content.temptext));
									RT_PlusShow = RT_MsgShow = rtp_itoggle = true;
								}
							}
							break;
						case 12:	// Info_News
							asprintf(&rtp_content.info_News, "%s", rtrim(rtp_content.temptext));
							break;
						case 13:	// Info_NewsLocal
							asprintf(&rtp_content.info_NewsLocal, "%s", rtrim(rtp_content.temptext));
							break;
						case 14:	// Info_Stockmarket
							if (++rtp_content.info_StockIndex >= MAX_RTPC)
								rtp_content.info_StockIndex = 0;
							asprintf(&rtp_content.info_Stock[rtp_content.info_StockIndex], "%s", rtrim(rtp_content.temptext));
							break;
						case 15:	// Info_Sport
							if (++rtp_content.info_SportIndex >= MAX_RTPC)
								rtp_content.info_SportIndex = 0;
							asprintf(&rtp_content.info_Sport[rtp_content.info_SportIndex], "%s", rtrim(rtp_content.temptext));
							break;
						case 16:	// Info_Lottery
							if (++rtp_content.info_LotteryIndex >= MAX_RTPC)
								rtp_content.info_LotteryIndex = 0;
							asprintf(&rtp_content.info_Lottery[rtp_content.info_LotteryIndex], "%s", rtrim(rtp_content.temptext));
							break;
						case 24:	// Info_DateTime
							asprintf(&rtp_content.info_DateTime, "%s", rtrim(rtp_content.temptext));
							break;
						case 25:	// Info_Weather
							if (++rtp_content.info_WeatherIndex >= MAX_RTPC)
								rtp_content.info_WeatherIndex = 0;
							asprintf(&rtp_content.info_Weather[rtp_content.info_WeatherIndex], "%s", rtrim(rtp_content.temptext));
							break;
						case 26:	// Info_Traffic
							asprintf(&rtp_content.info_Traffic, "%s", rtrim(rtp_content.temptext));
							break;
						case 27:	// Info_Alarm
							asprintf(&rtp_content.info_Alarm, "%s", rtrim(rtp_content.temptext));
							break;
						case 28:	// Info_Advert
							asprintf(&rtp_content.info_Advert, "%s", rtrim(rtp_content.temptext));
							break;
						case 29:	// Info_Url
							asprintf(&rtp_content.info_Url, "%s", rtrim(rtp_content.temptext));
							break;
						case 30:	// Info_Other
							if (++rtp_content.info_OtherIndex >= MAX_RTPC)
								rtp_content.info_OtherIndex = 0;
							asprintf(&rtp_content.info_Other[rtp_content.info_OtherIndex], "%s", rtrim(rtp_content.temptext));
							break;
						case 31:	// Programme_Stationname.Long
							asprintf(&rtp_content.prog_Station, "%s", rtrim(rtp_content.temptext));
							break;
						case 32:	// Programme_Now
							asprintf(&rtp_content.prog_Now, "%s", rtrim(rtp_content.temptext));
							break;
						case 33:	// Programme_Next
							asprintf(&rtp_content.prog_Next, "%s", rtrim(rtp_content.temptext));
							break;
						case 34:	// Programme_Part
							asprintf(&rtp_content.prog_Part, "%s", rtrim(rtp_content.temptext));
							break;
						case 35:	// Programme_Host
							asprintf(&rtp_content.prog_Host, "%s", rtrim(rtp_content.temptext));
							break;
						case 36:	// Programme_EditorialStaff
							asprintf(&rtp_content.prog_EditStaff, "%s", rtrim(rtp_content.temptext));
							break;
						case 38:	// Programme_Homepage
							asprintf(&rtp_content.prog_Homepage, "%s", rtrim(rtp_content.temptext));
							break;
						case 39:	// Phone_Hotline
							asprintf(&rtp_content.phone_Hotline, "%s", rtrim(rtp_content.temptext));
							break;
						case 40:	// Phone_Studio
							asprintf(&rtp_content.phone_Studio, "%s", rtrim(rtp_content.temptext));
							break;
						case 44:	// Email_Hotline
							asprintf(&rtp_content.email_Hotline, "%s", rtrim(rtp_content.temptext));
							break;
						case 45:	// Email_Studio
							asprintf(&rtp_content.email_Studio, "%s", rtrim(rtp_content.temptext));
							break;
					}
				}
			}

			// Title-end @ no Item-Running'
			if ((mtext[10] & 0x08) == 0)
			{
				sprintf(RTP_Title, "---");
				sprintf(RTP_Artist, "---");
				if (RT_PlusShow)
				{
					RT_PlusShow = false;
					rtp_itoggle = true;
					rtp_idiffs = (int) rtp_itime.Elapsed() / 1000;
					RTP_Starttime = time(NULL);
				}
				RT_MsgShow = (RT_Info > 0);
				rtp_content.item_New = false;
			}

			if (rtp_itoggle)
			{
				if (S_Verbose >= 1)
				{
					struct tm tm_store;
					struct tm *ts = localtime_r(&RTP_Starttime, &tm_store);
					if (rtp_idiffs > 0)
						printf("  StartTime : %02d:%02d:%02d  (last Title elapsed = %d s)\n",
							ts->tm_hour, ts->tm_min, ts->tm_sec, rtp_idiffs);
					else
						printf("  StartTime : %02d:%02d:%02d\n", ts->tm_hour, ts->tm_min, ts->tm_sec);
					printf("  RTp-Title : %s\n  RTp-Artist: %s\n", RTP_Title, RTP_Artist);
				}
				RTP_ItemToggle = mtext[10] & 0x10;
				rtp_itoggle = false;
				rtp_idiffs = 0;
				RadioStatusMsg();
			}
			RTP_TToggle = 0;
		}
	}
	else
	{
		if (S_Verbose >= 1)
			printf("RDS-Error: [RTDecode] Length not correct !\n");
	}
}

/**
 * Decode PS/PTYN text elements from UECP messages.
 */
void CRadioText::RDS_PsPtynDecode(bool ptyn, unsigned char *mtext, int len)
{
	if (len < 16)
		return;

	// decode Text
	for (int i = 8; i <= 15; i++)
	{
		if (mtext[i] <= 0xfe)
		{
			// additional rds-character, see RBDS-Standard, Annex E
			if (!ptyn)
				RDS_PSText[RDS_PSIndex][i - 8] = (mtext[i] >= 0x80) ? rds_addchar[mtext[i] - 0x80] : mtext[i];
			else
				RDS_PTYN[i - 8] = (mtext[i] >= 0x80) ? rds_addchar[mtext[i] - 0x80] : mtext[i];
		}
	}

	if (S_Verbose >= 1)
	{
		if (!ptyn)
			printf("RDS-PS  No= %d, Content[%d]= '%s'\n", mtext[7], RDS_PSIndex, RDS_PSText[RDS_PSIndex]);
		else
			printf("RDS-PTYN  No= %d, Content= '%s'\n", mtext[7], RDS_PTYN);
	}

	if (!ptyn)
	{
		RDS_PSIndex += 1; if (RDS_PSIndex >= 12) RDS_PSIndex = 0;
		RT_MsgShow = RDS_PSShow = true;
	}
}

/**
 * Emit current radiotext/RT+ info for LCD and status outputs.
 */
void CRadioText::RadioStatusMsg(void)
{
	/* announce text/items for lcdproc & other */
	if (!RT_MsgShow || S_RtMsgItems <= 0)
		return;

	if (S_RtMsgItems >= 2)
	{
		char temp[100];
		int ind = (RT_Index == 0) ? S_RtOsdRows - 1 : RT_Index - 1;
		strcpy(temp, RT_Text[ind]);
		printf("RadioStatusMsg = %s\n", temp);
	}

	if ((S_RtMsgItems == 1 || S_RtMsgItems >= 3) && ((S_RtOsdTags == 1 && RT_PlusShow) || S_RtOsdTags >= 2))
	{
		printf("RTP_Title = %s, RTP_Artist = %s\n", RTP_Title, RTP_Artist);
	}
}

CRadioText::CRadioText(void)
{
	pid = 0;
	audioDemux = NULL;
	init();

	running = true;
	start();
}

CRadioText::~CRadioText(void)
{
	printf("CRadioText::~CRadioText\n");
	running = false;
	radiotext_stop();
	cond.broadcast();
	OpenThreads::Thread::join();
	latm_dump_close();
	if (g_RadiotextWin)
	{
		delete g_RadiotextWin;
		g_RadiotextWin = NULL;
	}
	printf("CRadioText::~CRadioText done\n");
}

void CRadioText::init()
{
	S_Verbose 	= 0;
	S_RtFunc 	= 1;
	S_RtOsd 	= 0;
	S_RtOsdTitle 	= 1;
	S_RtOsdTags 	= 2;
	S_RtOsdPos 	= 2;
	S_RtOsdRows 	= 3;
	S_RtOsdLoop 	= 1;
	S_RtOsdTO 	= 60;
	S_RtSkinColor 	= false;
	S_RtBgCol 	= 0;
	S_RtBgTra 	= 0xA0;
	S_RtFgCol 	= 1;
	S_RtDispl 	= 1;
	S_RtMsgItems 	= 0;
	RT_Index 	= 0;
	RT_PTY 		= 0;

	// Radiotext
	RTP_ItemToggle 	= 1;
	RTP_TToggle 	= 0;
	RT_PlusShow 	= false;
	RT_Replay 	= false;
	RT_ReOpen 	= false;
	for (int i = 0; i < 5; i++)
		RT_Text[i][0] = 0;
	RDS_PTYN[0] = 0;

	RT_MsgShow = false; // clear entries from old channel
	have_radiotext	= false;
	uecp_in_frame = false;
	uecp_escape = false;
	uecp_index = -1;
	uecp_last_ts = 0;

	latm_cfg = LatmConfig();
	latm_pending.clear();
	latm_dse_hits = 0;
	latm_uecp_ok = 0;
	latm_uecp_crc_fail = 0;

	const char *rt_verbose = getenv("RADIOTEXT_VERBOSE");
	if (rt_verbose)
		S_Verbose = atoi(rt_verbose);
	latm_dump_setup(pid);
}

void CRadioText::radiotext_stop(void)
{
	printf("CRadioText::radiotext_stop: ###################### pid 0x%x ######################\n", getPid());
	if (getPid() != 0)
	{
		mutex.lock();
		pid = 0;
		have_radiotext = false;
		S_RtOsd = 0;
		mutex.unlock();
	}
}

void CRadioText::setPid(uint inPid)
{
	printf("CRadioText::setPid: ###################### old pid 0x%x new pid 0x%x ######################\n", pid, inPid);
	if (!g_RadiotextWin)
	{
		g_RadiotextWin = new CRadioTextGUI();
		g_RadiotextWin->allowPaint(false);
	}
	if (pid != inPid)
	{
		mutex.lock();
		pid = inPid;
		init();
		mutex.unlock();
		cond.broadcast();
	}
}

void CRadioText::run()
{
	set_threadname("n:radiotext");
	uint current_pid = 0;

	printf("CRadioText::run: ###################### Starting thread ######################\n");
#if HAVE_GENERIC_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	int buflen = 0;
	unsigned char *buf = NULL;
	audioDemux = new cDemux(0); // live demux
#else
	audioDemux = new cDemux(1);
#endif
	audioDemux->Open(DMX_PES_CHANNEL, 0, 128 * 1024);

	while (running)
	{
		mutex.lock();
		if (pid == 0)
		{
			mutex.unlock();
			audioDemux->Stop();
			pidmutex.lock();
			printf("CRadioText::run: ###################### waiting for pid.. ######################\n");
			cond.wait(&pidmutex);
			pidmutex.unlock();
			mutex.lock();
		}
		if (pid && (current_pid != pid))
		{
			current_pid = pid;
			printf("CRadioText::run: ###################### Setting PID 0x%x ######################\n", getPid());
			audioDemux->Stop();
			if (!audioDemux->pesFilter(getPid()) || !audioDemux->Start())
			{
				pid = 0;
				printf("CRadioText::run: ###################### failed to start PES filter ######################\n");
			}
		}
		mutex.unlock();
		if (pid)
		{
#if HAVE_GENERIC_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
			int n;
			unsigned char tmp[6];

			n = audioDemux->Read(tmp, 6, 500);
			if (n != 6)
			{
				usleep(10000); /* save CPU if nothing read */
				continue;
			}
			if (memcmp(tmp, "\000\000\001\300", 4))
			{
				mutex.lock();
				processLatmFromPes(tmp, n);
				mutex.unlock();
				continue;
			}
			int packlen = ((tmp[4] << 8) | tmp[5]) + 6;

			if (buflen < packlen)
			{
				if (buf)
					free(buf);
				buf = (unsigned char *) calloc(1, packlen);
				buflen = packlen;
			}
			if (!buf)
				break;
			memcpy(buf, tmp, 6);

			while ((n < packlen) && running)
			{
				int len = audioDemux->Read(buf + n, packlen - n, 500);
				if (len < 0)
					break;
				n += len;
			}
#else
			int n;
			unsigned char buf[0x1FFFF];

			n = audioDemux->Read(buf, sizeof(buf), 500 /*5000*/);
#endif
			if (n > 0)
			{
				//printf("."); fflush(stdout);
				mutex.lock();
				if (!processLatmFromPes(buf, n))
					PES_Receive(buf, n);
				mutex.unlock();
			}
		}
	}
#if HAVE_GENERIC_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	if (buf)
		free(buf);
#endif
	delete audioDemux;
	audioDemux = NULL;
	printf("CRadioText::run: ###################### exit ######################\n");
}
