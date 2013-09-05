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

enum Stream_ID {
	Pack_Header = 0xBA
};

struct MPEG_PS_HDR {
	uint32 start_code; // 0x000001<Stream_ID>
	unit32 marker_clock_bits;
	uint16 marker_clock_scr_bits;
	uint16 bit_rate;
	uint8  bit_rate_marker;
	uint8  reserved_stuffing_len; // stuffing len low 3 bits
};

struct PES_HDR {
};

PES_HDR *get_pes_hdr_from_struct(MPEG_PS_HDR *ps_hdr) {
	int stuffing_len = ps_hdr->reserved_stuffing_len & 0x03;
}

PES_HDR *get_pes_hdr(char *ps_hdr) {
	ps_hdr += 3;
	char stream_id = *ps_hdr++;
	printf("stream_id = 0x%x\n", stream_id);
	ps_hdr += 9;
	int stuffing_bytes = *ps_hdr++ & 0x03;
	printf("stuffing_bytes = %d\n", stuffing_bytes);
	ps_hdr += stuffing_bytes;
	if (*((uint32*)ps_hdr) == 0xBB010000) {
		printf("partial system header present\n");
		ps_hdr += 12;
	}
	return (PES_HDR*)ps_hdr;
}





#endif // !defined(MPEG_H)
