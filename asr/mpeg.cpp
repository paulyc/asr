// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "mpeg.h"

MPEGProcessorBase::MPEGProcessorBase(const char *in, FILE *outfile, uint8 substream_id) :
_eof(false),
_substream_id(substream_id)
{
	_f = fopen(in, "rb");
	_ac3_output = outfile;
}

MPEGProcessorBase::~MPEGProcessorBase()
{
	fclose(_f);
}

void MPEGProcessorBase::process()
{
	processFile();
}

void MPEGProcessorBase::processPrivateStream1()
{
	assert(_cur_hdr->stream_id == 0xBD);
	MPEG_PES_Header_Extension *ext = (MPEG_PES_Header_Extension*)(_cur_hdr+1);
	uint8 *substream = (uint8*)ext + ext->pes_header_data_length + 3;
	//printf("private stream 1 (non mpeg audio) with extension substream 0x%2x\n", *substream);
	
	const int frames = *(substream+1);
	const int offset = htons(*(short*)(substream+2));
	char *ac3dat = (char*)substream+4;
	const int bytes = htons(_cur_hdr->packet_length) - (ac3dat - (char*)ext);
	//	printf("0x%x\n",*substream);
	if (*substream == 0x20) {
		nextPESHdr();
	} else if (*substream == _substream_id) {
		processSubstreamData(ac3dat, bytes, true);
	} /*else if (*substream == 0x83 || *substream == 0x80) {
	   processSubstreamData(ac3dat, bytes, false);
	   }*/ else {
		   nextPESHdr();
	   }
}

void MPEGProcessorBase::processSubstreamData(char *dat, int bytes, bool write)
{
	_cur_hdr = (MPEG_PES_Header*)((char*)_cur_hdr + htons(_cur_hdr->packet_length) + 6);
	if (!write) return;
	
	//AC3_Frame_Hdr *ac3hdr = (AC3_Frame_Hdr*)dat;
	//int by = ac3_bytes_per_frame(ac3hdr->frminfo);
	
	int buffer_left = BUFFER_SIZE - ((char*)dat - _buffer);
	if (bytes <= buffer_left) {
		if (write) {
			fwrite(dat, 1, bytes, _ac3_output);
		}
	} else {
		if (write) {
			fwrite(dat, 1, buffer_left, _ac3_output);
		}
		bytes -= buffer_left;
		
		checkBuffer();
		
		if (write) {
			fwrite(_buffer, 1, bytes, _ac3_output);
		}
	}
}

void MPEGProcessorBase::processPrivateStream2()
{
	//	printf("private stream 2 (navigation) without extension\n");
	char substream = *(char*)(_cur_hdr+1);
	//	printf("substream 0x%x\n", substream);
}

void MPEGProcessorBase::processFile()
{
	_buffer_offset = 0;
	fread(_buffer, 1, BUFFER_SIZE, _f);
	_cur_hdr = (MPEG_PES_Header*)_buffer;
	
	while (checkBuffer()) {
		//	printf("packet length %d id %x\n", htons(_cur_hdr->packet_length), _cur_hdr->stream_id);
		if (_cur_hdr->stream_id == Pack_Header) {
			//	printf("packet length %d id %x\n", htons(_cur_hdr->packet_length), _cur_hdr->stream_id);
			_cur_hdr = get_pes_hdr_from_pack_hdr((MPEG_Partial_PS_Pack_Header*)_cur_hdr);
			continue;
		} else if (_cur_hdr->stream_id == Private_Stream_1_Header) {
			//	printf("packet length %d id %x\n", htons(_cur_hdr->packet_length), _cur_hdr->stream_id);
			processPrivateStream1();
			continue;
		}
		//	printf("packet length %d id %x\n", htons(_cur_hdr->packet_length), _cur_hdr->stream_id);
		switch (_cur_hdr->stream_id) {
			case 0xBE:
				//	printf("padding stream\n");
				break;
			case 0xBF:
				processPrivateStream2();
				break;
			case 0xE0:
				break;
			default:
				break;
		}
		//	if (_cur_hdr->stream_id >=0xC0 || _cur_hdr->stream_id <= 0xDF)
		//		printf("MPEG audio stream");
		nextPESHdr();
	}
}

bool MPEGProcessorBase::checkBuffer()
{
	if ((char*)_cur_hdr - _buffer > BUFFER_SIZE - sizeof(MPEG_PES_Header)) {
		int bytes_left = BUFFER_SIZE - ((char*)_cur_hdr-_buffer);
		if (_eof) {
			return false;
		}
		memcpy(_buffer, _cur_hdr, bytes_left);
		if (fread(_buffer+bytes_left, 1, BUFFER_SIZE-bytes_left, _f) < BUFFER_SIZE-bytes_left) {
			_eof = true;
		}
		_buffer_offset += BUFFER_SIZE-bytes_left;
		_cur_hdr = (MPEG_PES_Header*)((char*)_cur_hdr - BUFFER_SIZE + bytes_left);
	}
	assert(_cur_hdr->zero == 0x0000);
	assert(_cur_hdr->packet_start_code_prefix == 0x01);
	return true;
}

void MPEGProcessorBase::nextPESHdr()
{
	_cur_hdr = (MPEG_PES_Header*)((char*)_cur_hdr + htons(_cur_hdr->packet_length) + 6);
}

#if 0
int main(int argc, char* argv[])
{
	char filename[128];
	FILE *output = fopen("tng-borg.ac3", "wb");
	for (int i=1; i<=8; ++i) {
		snprintf(filename, sizeof(filename), "/Volumes/TNGS5D6/VIDEO_TS/VTS_02_%d.VOB", i);
		std::string str(filename);
		std::cout << "processing " << str << std::endl;
		MPEGProcessorBase proc(filename, output, 0x81);
		proc.process();
	}
	fclose(output);
	return 0;
}
#endif
