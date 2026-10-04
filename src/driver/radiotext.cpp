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
 * LATM/RDS decode path, taken only for audio the PMT gives as AAC in LATM
 * (stream type 0x11, passed in with setPid):
 * - PES audio payload -> LOAS sync (0x56, 0xe0 mask) -> AudioMuxElement
 * - the StreamMuxConfig is read in full, its AudioSpecificConfig with the
 *   GASpecificConfig included, so that the length of the access unit (AU)
 *   behind it, and the AU itself, are read from the right bit
 * - each AU is searched from its END backwards for the data stream element
 *   (DSE): ETSI TS 101 154 Annex C.5.1 puts it after the audio elements,
 *   before an optional FIL and the END. From the front it could only be
 *   found by decoding the audio elements in front of it.
 * - the DSE bytes carry UECP (EBU SPB 490) in forward order, one frame
 *   spread over several AUs: 0xfe start, 0xff end, 0xfd 00/01/02 stuffing,
 *   ADD SQC MFL MSG CRC, with the RDS messages (RT/RT+/PS/PTY/...)
 *
 * Debugging:
 * - RADIOTEXT_VERBOSE=0..3 prints UECP/MEC details, from 2 on also a line
 *   of counters at most every 10 s: frames lost show as sqc_gap, while
 *   drop counts frames given up, mostly ones started by DSE look-alikes
 * - RADIOTEXT_DUMP=1 or a path writes hex lines to /tmp/radiotext_dse.log:
 *   DSE = DSE data fed to the UECP reader, DSE_ANC = fed, but it may be
 *   MPEG-4 ancillary data as well, DSE_SKIP = DSE data left out (no frame
 *   open, no start byte in it), UECP = a frame with a right CRC, UECP_CRC =
 *   a frame with a wrong one
 * - RADIOTEXT_DUMP_MAX limits output size (default 524288)
 */
struct LatmConfig
{
	bool valid;         // a StreamMuxConfig has been read ...
	bool unsupported;   // ... but it describes audio this reader cannot walk
	int num_sub_frames;
	int frame_length_type;
	int frame_length;   // AU bytes with frame_length_type 1

	LatmConfig()
		: valid(false)
		, unsupported(false)
		, num_sub_frames(0)
		, frame_length_type(0)
		, frame_length(0)
	{
	}
};

static LatmConfig latm_cfg;
static std::vector<unsigned char> latm_pending;
static const size_t latm_pending_max = 8192;
static std::vector<unsigned char> latm_au;
static int latm_aus = 0;
static int latm_dse_hits = 0;
static int latm_dse_fed = 0;
static int latm_uecp_ok = 0;
static int latm_uecp_crc_fail = 0;
static int latm_uecp_drop = 0;
static int latm_uecp_sqc_gap = 0;
static int latm_uecp_last_sqc = 0;
static time_t latm_stats_ts = 0;
static FILE *latm_dump_fp = NULL;
static unsigned int latm_dump_bytes = 0;
static unsigned int latm_dump_limit = 0;
static bool latm_dump_enabled = false;
static char latm_dump_path[256];
static const unsigned int latm_dump_default_limit = 512 * 1024;

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

		int position() const
		{
			return bitpos;
		}

		int getBits(int n)
		{
			if (n <= 0 || n > bitlen - bitpos)
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
			if (n < 0 || n > bitlen - bitpos)
				return false;
			bitpos += n;
			return true;
		}

		bool readBytes(unsigned char *out, int count)
		{
			if (count < 0 || count > (bitlen - bitpos) / 8)
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
 * Read an audio object type: 5 bits, where 31 escapes to 32 + 6 bits.
 */
static int latm_get_audio_object_type(LatmBitReader &br)
{
	int aot = br.getBits(5);
	if (aot == 31)
	{
		int ext = br.getBits(6);
		aot = ext < 0 ? -1 : 32 + ext;
	}
	return aot;
}

/**
 * Skip a sampling frequency index, and the frequency an index of 0xf
 * brings along.
 */
static bool latm_skip_sampling_frequency(LatmBitReader &br)
{
	int index = br.getBits(4);
	return index >= 0 && (index != 0x0f || br.skipBits(24));
}

/**
 * Read an AudioSpecificConfig (ISO/IEC 14496-3 1.6.2.1) up to its end, its
 * GASpecificConfig included; the StreamMuxConfig goes on right behind it.
 * Only AAC Main, LC, SSR and LTP, with or without SBR/PS, have AUs made of
 * a raw_data_block() with the DSE in it. Any other object type, and the
 * program_config_element of channel configuration 0, set unsupported.
 */
static bool latm_read_audio_specific_config(LatmBitReader &br, bool &unsupported)
{
	int aot = latm_get_audio_object_type(br);
	if (aot < 0 || !latm_skip_sampling_frequency(br))
		return false;
	int channel_config = br.getBits(4);
	if (channel_config < 0)
		return false;
	if (aot == 5 || aot == 29)
	{
		// SBR, PS: the extension sampling frequency, then the core type
		if (!latm_skip_sampling_frequency(br))
			return false;
		aot = latm_get_audio_object_type(br);
		if (aot < 0)
			return false;
	}
	if (aot < 1 || aot > 4 || channel_config == 0)
	{
		unsupported = true;
		return true;
	}

	// GASpecificConfig
	if (br.getBits(1) < 0)                  // frameLengthFlag
		return false;
	int depends_on_core_coder = br.getBits(1);
	if (depends_on_core_coder < 0)
		return false;
	if (depends_on_core_coder && !br.skipBits(14))  // coreCoderDelay
		return false;
	int extension_flag = br.getBits(1);
	if (extension_flag < 0)
		return false;
	if (extension_flag && br.getBits(1) < 0)    // extensionFlag3
		return false;
	return true;
}

/**
 * Read a StreamMuxConfig (ISO/IEC 14496-3 1.7.3.1). A stream this reader
 * cannot walk comes back valid but unsupported: audioMuxVersionA 1, more
 * than one program or layer, subframes not framed alike, AUs that are not
 * AAC raw_data_block()s, and the CELP/HVXC frame length types.
 */
static bool latm_read_stream_mux_config(LatmBitReader &br, LatmConfig &cfg)
{
	cfg = LatmConfig();
	int audio_mux_version = br.getBits(1);
	if (audio_mux_version < 0)
		return false;
	if (audio_mux_version)
	{
		int audio_mux_version_A = br.getBits(1);
		if (audio_mux_version_A < 0)
			return false;
		if (audio_mux_version_A)
		{
			cfg.valid = cfg.unsupported = true;
			return true;
		}
		bool ok = false;
		latm_get_value(br, ok);             // taraBufferFullness
		if (!ok)
			return false;
	}

	int all_same_time_framing = br.getBits(1);
	cfg.num_sub_frames = br.getBits(6);
	int num_program = br.getBits(4);
	int num_layer = br.getBits(3);
	if (all_same_time_framing < 0 || cfg.num_sub_frames < 0 || num_program < 0 || num_layer < 0)
		return false;
	if (!all_same_time_framing || num_program || num_layer)
	{
		cfg.valid = cfg.unsupported = true;
		return true;
	}

	unsigned int asc_len = 0;
	if (audio_mux_version)
	{
		bool ok = false;
		asc_len = latm_get_value(br, ok);
		if (!ok || asc_len > (unsigned int)br.bitsLeft())
			return false;
	}
	int asc_start = br.position();
	if (!latm_read_audio_specific_config(br, cfg.unsupported))
		return false;
	if (cfg.unsupported)
	{
		cfg.valid = true;
		return true;
	}
	if (audio_mux_version)
	{
		// the config may hold more than was read, a sync extension say
		unsigned int used = br.position() - asc_start;
		if (used > asc_len || !br.skipBits((int)(asc_len - used)))
			return false;
	}

	cfg.frame_length_type = br.getBits(3);
	if (cfg.frame_length_type < 0)
		return false;
	if (cfg.frame_length_type == 0)
	{
		if (br.getBits(8) < 0)              // latmBufferFullness
			return false;
	}
	else if (cfg.frame_length_type == 1)
	{
		int frame_length = br.getBits(9);
		if (frame_length < 0)
			return false;
		cfg.frame_length = frame_length + 20;   // 8 * (frameLength + 20) bits
	}
	else
	{
		cfg.valid = cfg.unsupported = true;
		return true;
	}

	int other_data_present = br.getBits(1);
	if (other_data_present < 0)
		return false;
	if (other_data_present)
	{
		// only the length is here, the other data follows the AUs
		if (audio_mux_version)
		{
			bool ok = false;
			latm_get_value(br, ok);         // otherDataLenBits
			if (!ok)
				return false;
		}
		else
		{
			int esc;
			do
			{
				esc = br.getBits(1);
				if (esc < 0 || br.getBits(8) < 0)   // otherDataLenTmp
					return false;
			}
			while (esc);
		}
	}
	int crc_check_present = br.getBits(1);
	if (crc_check_present < 0 || (crc_check_present && br.getBits(8) < 0))  // crcCheckSum
		return false;

	cfg.valid = true;
	return true;
}

/**
 * Read a PayloadLengthInfo: the length in bytes of the AU that follows.
 */
static int latm_read_payload_length_info(LatmBitReader &br, const LatmConfig &cfg)
{
	if (cfg.frame_length_type == 1)
		return cfg.frame_length;
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

/**
 * Read n bits (at most 24) at bit pos of buf, MSB first. The caller keeps
 * pos + n inside the buffer.
 */
static unsigned int latm_peek_bits(const unsigned char *buf, int len, int pos, int n)
{
	int byte = pos >> 3;
	unsigned int val = 0;
	for (int i = 0; i < 4; i++)
		val = (val << 8) | (byte + i < len ? buf[byte + i] : 0);
	return (val >> (32 - (pos & 7) - n)) & ((1u << n) - 1);
}

struct LatmDse
{
	int header;         // bit where its header starts in the AU
	int start;          // bit where its data starts
	int count;          // data bytes
	bool starts_frame;  // the data starts with the UECP start byte 0xfe
	bool has_start;     // ... or holds it somewhere

	LatmDse()
		: header(0)
		, start(0)
		, count(0)
		, starts_frame(false)
		, has_start(false)
	{
	}
};

/**
 * Rank of a DSE hit while no UECP frame is open: then only a frame start
 * counts, and the first piece of a frame starts with its start byte.
 */
static int latm_dse_rank(const LatmDse &dse, bool in_frame)
{
	if (in_frame)
		return 0;
	return dse.starts_frame ? 2 : dse.has_start ? 1 : 0;
}

/**
 * Find the DSE of an AU, a raw_data_block(), from its END backwards.
 *
 * The DSE stands right before the END, or before one FIL in front of the
 * END, and has the tag of the first audio element (TS 101 154 C.5.1). That
 * gives the end of its data, and for each count the one place its header
 * must be at, where it has to read ID_DSE, the tag, the align flag and that
 * very count; with the align flag the header may end up to 7 bits before
 * data that starts on a byte. A byte counts from the start of the AU, the
 * way ISO/IEC 14496-3 has it, or from the start of the AudioMuxElement, the
 * way FFmpeg and FAAD2 read it: au_shift is the AU's bit offset in there.
 *
 * More than one hit is rare: one inside the data of the real DSE cuts off
 * its front, one in the audio data before it adds a front of garbage. With
 * no UECP frame open (in_frame false), data starting with the UECP start
 * byte comes first, then data holding one; after that the hit nearest to
 * the END wins. MPEG-4 ancillary data (TS 101 154 C.5.2), which may stand
 * in that place now and then, is sorted out by the UECP reader.
 */
static bool latm_find_dse(const unsigned char *au, int len, int au_shift, bool in_frame, LatmDse &out)
{
	// the first element: SCE, CPE or LFE, and its tag
	if (len < 3)
		return false;
	unsigned int first_id = latm_peek_bits(au, len, 0, 3);
	if (first_id != 0 && first_id != 1 && first_id != 3)
		return false;
	unsigned int tag = latm_peek_bits(au, len, 3, 4);

	// ID_END: the last three bits before the zeros that align the AU
	int last = len - 1;
	while (last >= 0 && !au[last])
		last--;
	if (last < 0)
		return false;
	int end = last * 8 + 7;
	for (unsigned char b = au[last]; !(b & 1); b >>= 1)
		end--;
	end -= 2;
	if (end < 7 || latm_peek_bits(au, len, end, 3) != 7)
		return false;

	// where the DSE data may end: at the END, or at a FIL before the END
	int tails[1 + 15 + 256];
	int num_tails = 0;
	tails[num_tails++] = end;
	for (int cnt = 0; cnt < 15; cnt++)
	{
		int q = end - 7 - 8 * cnt;
		if (q < 7)
			break;
		if (latm_peek_bits(au, len, q, 7) == ((6u << 4) | cnt))
			tails[num_tails++] = q;
	}
	for (int esc = 0; esc < 256; esc++)
	{
		// a FIL count of 15 escapes to 15 + esc - 1 bytes
		int q = end - 15 - 8 * (14 + esc);
		if (q < 7)
			break;
		if (latm_peek_bits(au, len, q, 15) == ((6u << 12) | (15u << 8) | esc))
			tails[num_tails++] = q;
	}

	bool found = false;
	for (int t = 0; t < num_tails; t++)
	{
		int q = tails[t];
		for (int count = 1; count <= 510; count++)
		{
			int start = q - 8 * count;
			int hlen = count < 255 ? 16 : 24;
			if (start - hlen < 7)
				break;
			for (int align = 0; align < 2; align++)
			{
				int gaps = 1;
				if (align)
				{
					if (start % 8 && (start + au_shift) % 8)
						continue;
					gaps = 8;
				}
				// ID_DSE, tag, align flag, count (255 + escape)
				unsigned int head = (4u << 5) | (tag << 1) | align;
				unsigned int want = count < 255 ? (head << 8) | count : (head << 16) | (255u << 8) | (count - 255);
				for (int gap = 0; gap < gaps; gap++)
				{
					int header = start - gap - hlen;
					if (header < 7)
						break;
					if (latm_peek_bits(au, len, header, hlen) != want)
						continue;
					LatmDse dse;
					dse.header = header;
					dse.start = start;
					dse.count = count;
					dse.starts_frame = latm_peek_bits(au, len, start, 8) == 0xfe;
					for (int i = 0; i < count && !dse.has_start; i++)
						dse.has_start = latm_peek_bits(au, len, start + 8 * i, 8) == 0xfe;
					int rank = latm_dse_rank(dse, in_frame);
					bool better;
					if (!found)
						better = true;
					else if (rank != latm_dse_rank(out, in_frame))
						better = rank > latm_dse_rank(out, in_frame);
					else
						better = dse.header > out.header;
					if (better)
					{
						out = dse;
						found = true;
					}
				}
			}
		}
	}
	return found;
}

/**
 * Whether DSE data is well-formed MPEG-4 ancillary data (TS 101 154 C.5.2):
 * the sync byte 0xbc, bs_info and ancillary_data_status with their reserved
 * bits clear, then exactly the fields the status announces, optionally
 * followed by announcement switching data (C.5.3: 0xad and its length).
 */
static bool latm_is_ancillary_data(const unsigned char *data, int count)
{
	if (count < 3 || data[0] != 0xbc || (data[1] & 0x03) || (data[2] & 0xe8))
		return false;
	int len = 3 + (data[2] & 0x10 ? 1 : 0)  // downmixing_levels_MPEG4
		+ (data[2] & 0x04 ? 2 : 0)          // audio_coding_mode, Compression_value
		+ (data[2] & 0x02 ? 2 : 0)          // coarse_grain_timecode
		+ (data[2] & 0x01 ? 2 : 0);         // fine_grain_timecode
	if (count >= len + 2 && data[len] == 0xad)
		len += 2 + data[len + 1];
	return count == len;
}

// what a byte did to a reading of a UECP frame
enum
{
	UECP_IDLE,      // no frame open, byte passed over
	UECP_MORE,      // taken into the open frame
	UECP_START,     // 0xfe: a frame starts
	UECP_RESTART,   // 0xfe: a frame starts, the open one is given up
	UECP_END,       // 0xff: the open frame ends
	UECP_DROP       // the open frame is given up
};

} // namespace

/**
 * Look for the DSE of one AU and feed its bytes to the UECP reader: all of
 * them while a frame is open, otherwise only when they hold a start byte.
 */
void CRadioText::latm_scan_dse(const unsigned char *au, int len, int au_shift)
{
	bool in_frame = uecpOpen();
	LatmDse dse;
	if (!latm_find_dse(au, len, au_shift, in_frame, dse))
		return;
	unsigned char data[510];
	for (int i = 0; i < dse.count; i++)
		data[i] = (unsigned char)latm_peek_bits(au, len, dse.start + 8 * i, 8);
	latm_dse_hits++;
	bool feed = in_frame || dse.has_start;
	bool maybe_ancillary = latm_is_ancillary_data(data, dse.count);
	latm_dump_write_line(!feed ? "DSE_SKIP" : maybe_ancillary ? "DSE_ANC" : "DSE", data, dse.count, pid);
	if (!feed)
		return;
	latm_dse_fed++;
	processUecpBuffer(data, dse.count, maybe_ancillary);
}

/**
 * Read one AudioMuxElement (ISO/IEC 14496-3 1.7.3) and search each of its
 * AUs for a DSE. An AU starts at any bit; it is copied to a buffer of its
 * own, where it starts on a byte.
 */
void CRadioText::latm_process_frame(const unsigned char *data, int len)
{
	LatmBitReader br(data, len);
	int use_same_mux = br.getBits(1);
	if (use_same_mux < 0)
		return;
	if (!use_same_mux)
	{
		/* A configuration that cannot be read, a bit error say, costs this
		   frame only: the frames after it go on with the last good one. */
		LatmConfig cfg;
		if (!latm_read_stream_mux_config(br, cfg))
			return;
		if (cfg.unsupported && !(latm_cfg.valid && latm_cfg.unsupported) && S_Verbose >= 1)
			printf("RDS-LATM: audio configuration not supported, no radio text read from it\n");
		latm_cfg = cfg;
	}
	if (!latm_cfg.valid || latm_cfg.unsupported)
		return;

	for (int i = 0; i <= latm_cfg.num_sub_frames; i++)
	{
		int au_len = latm_read_payload_length_info(br, latm_cfg);
		if (au_len <= 0 || au_len > br.bitsLeft() / 8)
			return;
		int au_shift = br.position() & 7;
		latm_au.resize(au_len);
		if (!br.readBytes(&latm_au[0], au_len))
			return;
		latm_aus++;
		latm_scan_dse(&latm_au[0], au_len, au_shift);
	}
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
					keep_from = i;
					break;
				}
				latm_seen = true;
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

	if (S_Verbose >= 2)
	{
		time_t now = time(NULL);
		if (now < latm_stats_ts || now - latm_stats_ts >= 10)
		{
			latm_stats_ts = now;
			printf("RDS-LATM: aus %d dse %d (fed %d) uecp ok %d crc_fail %d drop %d sqc_gap %d\n",
				latm_aus, latm_dse_hits, latm_dse_fed, latm_uecp_ok, latm_uecp_crc_fail,
				latm_uecp_drop, latm_uecp_sqc_gap);
		}
	}

	return latm_seen;
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
		default:
			if (S_Verbose >= 2)
				printf("(RDS-MEC '%02x' not used)\n", mec);
			break;
	}
}

/**
 * Take one byte into a reading of a UECP frame (EBU SPB 490, 2.2).
 * Stuffing keeps 0xfe and 0xff out of the frame body, so a raw 0xfe always
 * starts a frame and a raw 0xff always ends it.
 */
int CRadioText::feedUecpByte(UecpFrame &frame, unsigned char val)
{
	if (val == 0xfe)
	{
		int result = frame.in_frame && frame.index > 0 ? UECP_RESTART : UECP_START;
		frame.in_frame = true;
		frame.escape = false;
		frame.index = 0;
		frame.buf[0] = val;
		return result;
	}
	if (!frame.in_frame)
		return UECP_IDLE;
	if (val == 0xff)
	{
		frame.in_frame = false;
		return UECP_END;
	}
	if (frame.escape)
	{
		frame.escape = false;
		if (val > 0x02)
		{
			// only 0xfd 00/01/02 stand for 0xfd/0xfe/0xff
			frame.in_frame = false;
			return UECP_DROP;
		}
		val += 0xfd;
	}
	else if (val == 0xfd)
	{
		frame.escape = true;
		return UECP_MORE;
	}
	frame.buf[++frame.index] = val;
	// ADD SQC MFL MSG CRC: never longer than its MFL makes it
	if (frame.index >= 4 && frame.index > frame.buf[4] + 6)
	{
		frame.in_frame = false;
		return UECP_DROP;
	}
	return UECP_MORE;
}

/**
 * Whether a frame ended by a raw 0xff is whole: its length fits its MFL and
 * its CRC is right (CCITT over ADD..MSG, SPB 490 2.2.7).
 */
bool CRadioText::uecpFrameOk(UecpFrame &frame)
{
	int n = frame.index;
	if (frame.escape || n < 6 || n != frame.buf[4] + 6)
		return false;
	return crc16_ccitt(frame.buf, n - 2, true) == ((frame.buf[n - 1] << 8) | frame.buf[n]);
}

/**
 * Hand a whole frame on to the MEC dispatch, as 0xfe ADD..CRC 0xff.
 */
void CRadioText::deliverUecpFrame(UecpFrame &frame)
{
	int n = frame.index;
	frame.buf[n + 1] = 0xff;
	latm_uecp_ok++;
	latm_dump_write_line("UECP", frame.buf, n + 2, pid);
	/* SQC 0 means unused, else it counts 1..255, 1 again after 255.
	   A repeat keeps its frame's number and comes before the count is 100
	   ahead (SPB 490 2.2.4), so up to 99 behind: no gap, and no new count. */
	int sqc = frame.buf[3];
	if (sqc)
	{
		int ahead = latm_uecp_last_sqc ? (sqc - latm_uecp_last_sqc + 255) % 255 : 1;
		if (ahead > 1 && ahead <= 255 - 100)
		{
			latm_uecp_sqc_gap++;
			if (S_Verbose >= 2)
				printf("RDS-UECP: sequence %02x after %02x, frames lost in between\n", sqc, latm_uecp_last_sqc);
		}
		if (ahead >= 1 && ahead <= 255 - 100)
			latm_uecp_last_sqc = sqc;
	}
	handleRdsMessage(frame.buf, n + 2);
}

/**
 * Count a frame given up: one that ended with the right length but a
 * wrong CRC, or one cut short.
 */
void CRadioText::failUecpFrame(UecpFrame &frame, bool ended)
{
	int n = frame.index;
	if (!ended || frame.escape || n < 6 || n != frame.buf[4] + 6)
	{
		latm_uecp_drop++;
		return;
	}
	frame.buf[n + 1] = 0xff;
	latm_uecp_crc_fail++;
	latm_dump_write_line("UECP_CRC", frame.buf, n + 2, pid);
	if (S_Verbose >= 1)
		printf("RDS-Error: wrong CRC # calc = %04x <> transmit = %02x%02x\n",
			crc16_ccitt(frame.buf, n - 2, true), frame.buf[n - 1], frame.buf[n]);
}

/**
 * Whether one of the readings has a frame open.
 */
bool CRadioText::uecpOpen() const
{
	for (int r = 0; r < uecp_readings; r++)
		if (uecp[r].in_frame)
			return true;
	return false;
}

/**
 * Go on with one reading: reading r, or an idle one for r < 0.
 */
void CRadioText::keepUecpReading(int r)
{
	if (r > 0)
		uecp[0] = uecp[r];
	else if (r < 0)
	{
		uecp[0].in_frame = false;
		uecp[0].escape = false;
		uecp[0].index = -1;
	}
	uecp_readings = 1;
}

/**
 * Drop the readings that add no way of reading what follows: one with no
 * frame open while another has one (it waits for a 0xfe, which starts the
 * same frame in all of them), and one the same as a reading before it.
 */
void CRadioText::pruneUecpReadings()
{
	if (!uecpOpen())
	{
		keepUecpReading(-1);
		return;
	}
	int n = 0;
	for (int r = 0; r < uecp_readings; r++)
	{
		if (!uecp[r].in_frame)
			continue;
		bool same = false;
		for (int k = 0; k < n && !same; k++)
			same = uecp[k].escape == uecp[r].escape && uecp[k].index == uecp[r].index &&
				memcmp(uecp[k].buf, uecp[r].buf, uecp[r].index + 1) == 0;
		if (same)
			continue;
		if (n != r)
			uecp[n] = uecp[r];
		n++;
	}
	uecp_readings = n;
}

/**
 * Take one byte into the readings from first on; the ones before it leave
 * out the DSE the byte comes in. Returns true when the byte ended a frame
 * whole: that reading is then the only one, and the frame is handed on.
 * Inside such a DSE nothing else is decided; otherwise a 0xfe starts the
 * same frame in every reading and a 0xff ends every open one, and a frame
 * given up is counted once, when no reading has it open any more.
 */
bool CRadioText::feedUecpReadings(unsigned char val, int first)
{
	bool was_open = uecpOpen();
	bool restart = false;
	int whole = -1, sized = -1, ended = -1;
	for (int r = first; r < uecp_readings; r++)
	{
		int result = feedUecpByte(uecp[r], val);
		if (result == UECP_RESTART)
			restart = true;
		if (result != UECP_END)
			continue;
		if (whole < 0 && uecpFrameOk(uecp[r]))
			whole = r;
		if (ended < 0)
			ended = r;
		if (sized < 0 && !uecp[r].escape && uecp[r].index >= 6 && uecp[r].index == uecp[r].buf[4] + 6)
			sized = r;
	}
	if (whole >= 0)
	{
		keepUecpReading(whole);
		deliverUecpFrame(uecp[0]);
		return true;
	}
	if (first > 0)
		return false;
	if (val == 0xfe)
	{
		if (restart)
			failUecpFrame(uecp[0], false);
		keepUecpReading(0);
	}
	else if (val == 0xff)
	{
		// the right length and a wrong CRC is a CRC error, else a frame cut short
		if (ended >= 0)
			failUecpFrame(uecp[sized >= 0 ? sized : ended], true);
		keepUecpReading(-1);
	}
	else if (was_open && !uecpOpen())
		failUecpFrame(uecp[0], false);
	return false;
}

/**
 * Assemble UECP frames from the bytes of the DSEs; one frame may spread
 * over many DSEs. maybe_ancillary marks a DSE that is well-formed MPEG-4
 * ancillary data (TS 101 154 C.5.2) as well, which may be sent now and
 * then in between, while a piece of UECP may look just the same. Every
 * reading then goes on twice, without the DSE and with it, so that a frame
 * holding pieces of both kinds has a reading too; at the next end of a
 * frame the one with the right length and CRC wins. There is room for
 * UECP_READINGS: past it no more copies are made, while the reading that
 * leaves every such DSE out always stays, so only a frame with an unusually
 * dense mix of the two can be lost.
 */
void CRadioText::processUecpBuffer(const unsigned char *data, int len, bool maybe_ancillary)
{
	int first = 0;
	if (maybe_ancillary)
	{
		first = uecp_readings;
		int copies = uecp_readings;
		if (copies > UECP_READINGS - uecp_readings)
			copies = UECP_READINGS - uecp_readings;
		if (copies < uecp_readings && S_Verbose >= 2)
			printf("RDS-UECP: %d readings of the open frame, %d of them not split again\n",
				uecp_readings, uecp_readings - copies);
		for (int r = 0; r < copies; r++)
			uecp[first + r] = uecp[r];
		uecp_readings += copies;
	}
	for (int i = 0; i < len; i++)
		if (feedUecpReadings(data[i], first))
			first = 0;	// the DSE was UECP: the rest of it goes on in one reading
	pruneUecpReadings();
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
	latm_stream = false;
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
	pidmutex.lock();
	cond.broadcast();
	pidmutex.unlock();
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
	keepUecpReading(-1);

	latm_cfg = LatmConfig();
	latm_pending.clear();
	latm_aus = 0;
	latm_dse_hits = 0;
	latm_dse_fed = 0;
	latm_uecp_ok = 0;
	latm_uecp_crc_fail = 0;
	latm_uecp_drop = 0;
	latm_uecp_sqc_gap = 0;
	latm_uecp_last_sqc = 0;
	latm_stats_ts = time(NULL);

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

void CRadioText::setPid(uint inPid, bool latm)
{
	printf("CRadioText::setPid: ###################### old pid 0x%x new pid 0x%x ######################\n", pid, inPid);
	if (!g_RadiotextWin)
	{
		g_RadiotextWin = new CRadioTextGUI();
		g_RadiotextWin->allowPaint(false);
	}
	if (pid != inPid || latm_stream != latm)
	{
		mutex.lock();
		pid = inPid;
		latm_stream = latm;
		init();
		mutex.unlock();
		pidmutex.lock();
		cond.broadcast();
		pidmutex.unlock();
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
			/* The filter is off now, so whatever PID comes next has to
			   set it up again, the one it had before included. */
			current_pid = 0;
			pidmutex.lock();
			printf("CRadioText::run: ###################### waiting for pid.. ######################\n");
			/* Looked at again under pidmutex, which setPid and the
			   destructor signal under: a PID set, or the end asked for,
			   between the look above and this wait would otherwise
			   signal nobody, and the thread would sleep on until the next
			   PID. The PID itself is read under its own mutex. */
			for (;;)
			{
				mutex.lock();
				bool idle = running && pid == 0;
				mutex.unlock();
				if (!idle)
					break;
				cond.wait(&pidmutex);
			}
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
				/* Not the start of an audio PES packet: only LATM audio
				   has anything to look for in it. */
				mutex.lock();
				if (latm_stream)
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
				/* LATM frames only on LATM audio. Told from the content alone,
				   a sync word turning up at random in MPEG audio set the
				   LATM configuration valid, which then held for every packet
				   until the next PID, and MPEG audio's own radio text was not
				   read any more. */
				mutex.lock();
				if (latm_stream)
					processLatmFromPes(buf, n);
				else
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
