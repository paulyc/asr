// ASR - Digital Signal Processor
// Copyright (C) 2002-2013	Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

#ifndef _MPEG_H
#define _MPEG_H

#include <cstdint>
#include <cstdio>
#include <cassert>
#include <string>
#include <iostream>

typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

const int BUFFER_SIZE = 4096;

enum Stream_ID {
	Pack_Header				= 0xBA,
	Private_Stream_1_Header = 0xBD,
	Padding_Stream			= 0xBE,
	Private_Stream_2		= 0xBF
};

//#define htons(b) (((b & 0xFF00) >> 8) | ((b & 0xFF) << 8))

#pragma pack(1)

struct MPEG_Partial_PS_Pack_Header {
	/*	uint32 start_code_prefix            : 24; // 0x000001
	 uint32 stream_id                    :  8;
	 */	uint32 start_code_prefix_stream_id;
	/*	uint32 marker_bits_1                :  2;
	 uint32 system_clock_reference_32_30 :  3;
	 uint32 marker_bit_1                 :  1; // 1
	 uint32 system_clock_reference_29_15 : 15;
	 uint32 marker_bit_2                 :  1; // 1
	 uint32 system_clock_reference_14_0  : 15;
	 uint32 marker_bit_3                 :  1; // 1
	 uint32 system_clock_reference_ext   :  9;
	 uint32 marker_bit_4                 :  1; // 1
	 */	uint32 garbage_1;
	uint16 garbage_2;
	/*	uint32 bit_rate                     : 22;
	 uint32 marker_bits_2                :  2; // 3
	 */	uint16 bit_rate_high;
	uint8  bit_rate_low_marker;
	/*	uint8 reserved                     :  5;
	 uint8 stuffing_length              :  3;
	 */	uint8  reserved_stuffing_length;
};

struct MPEG_Partial_PS_System_Header {
	uint32 sync; // 0xBB010000 in little endian
	uint16 header_len;
	uint32 rate_bound_marker_audio_bound_flags;
	uint8  flags_marker_video_bound;
	uint8  packet_rate_restriction_reserved;
};

struct MPEG_PES_Header {
	uint16 zero;
	uint8 packet_start_code_prefix; // 0x01
	uint8 stream_id;
	uint16 packet_length;
};

struct MPEG_PES_Header_Extension {
	uint8 junk1;
	uint8 junk2; // extension flag = junk2 & 0x01
	uint8 pes_header_data_length; //  length of the remainder of the PES header
};

struct DVD_Audio_Substream_Header {
	uint8 frame_count; // number of frames which begin in this packet
	uint16 frame_offset; // offset from last byte of this where PTS frame begins
};

struct AC3_Frame_Hdr {
	uint16 syncword; // 0x770B LE
	uint16 crc1;
	uint8  frminfo;
};

inline MPEG_PES_Header *get_pes_hdr_from_pack_hdr(MPEG_Partial_PS_Pack_Header *ps_hdr) {
	int stuffing_bytes = ps_hdr->reserved_stuffing_length & 3;
	return (MPEG_PES_Header*)((char*)ps_hdr + sizeof(MPEG_Partial_PS_Pack_Header) + stuffing_bytes);
}

inline int ac3_sampling_rate(uint8 fscod) {
	switch (fscod) {
		case 0:
			return 48000;
		case 1:
			return 44100;
		case 2:
			return 32000;
		case 3:
		default:
			printf("error: bad or reserved fscod value\n");
			return -1;
	}
}

inline int ac3_bytes_per_frame(uint8 frminfo) {
	switch (frminfo) {
		case 0x1E:
		case 0x1F:
			return 2*896;
			break;
		default:
			printf("error: dont know bytes per frame\n");
			return -1;
			break;
	}
}

class MPEGProcessorBase
{
public:
	MPEGProcessorBase(const char *in, FILE *outfile, uint8 substream_id);
	virtual ~MPEGProcessorBase();
	
	void process();
	
protected:
	virtual void processPrivateStream1();
	virtual void processPrivateStream2();
	
	virtual void processSubstreamData(char *dat, int bytes, bool write);
private:
	void processFile();
	
	void nextPESHdr();
	bool checkBuffer();
	
	bool checkBuffer(void **pPtr);
	
	FILE *_f;
	char _buffer[BUFFER_SIZE];
	unsigned int _buffer_offset;
	
	MPEG_PES_Header *_cur_hdr;
	bool _eof;
	
	FILE *_ac3_output;
	uint8 _substream_id;
};

#endif // !defined(MPEG_H)
