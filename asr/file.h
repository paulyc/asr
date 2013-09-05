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

#ifndef _FILE_H
#define _FILE_H

#include <cstdio>

class GenericFile
{
protected:
	bool _eof;
public:
	GenericFile() : 
	  _eof(false)
	{}
	
	virtual ~GenericFile() {}

	virtual size_t read(void *DstBuf, size_t ElementSize, size_t Count) = 0;
	virtual void seek(long ofs) = 0;
	virtual long tell() = 0;
	virtual bool eof()
	{
		return _eof;
	}
};

template <typename ElementT>
class TestFile : public GenericFile
{
public:
	TestFile(...) :
		_count(INT_MAX/2)
	{
	}
	void seek(long ofs)
	{
	}
	long tell()
	{
		return 0;
	}
	size_t read(void *DstBuf, size_t ElementSize, size_t Count)
	{
		size_t rd = 0;
		ElementT *buf = (ElementT*)DstBuf;
		for (;rd < Count; ++rd)
		{
			float foo = sinf(2.0f*float(M_PI)*440.0f*float(_count++));
			*buf++ = int(foo * float(INT_MAX));
		}
		return rd;
	}
	ElementT _count;
};

class DiskFile : public GenericFile
{
public:
	DiskFile(const char *filename, const char *mode="rb")
	{
		_File = fopen(filename, mode);
		if (_File == 0)
			throw std::runtime_error("file open failed");
	}
	~DiskFile()
	{
		fclose(_File);
	}
	void seek(long ofs)
	{
		fseek(_File, ofs, 0);
	}
	long tell()
	{
		return ftell(_File);
	}
	size_t read(void *DstBuf, size_t ElementSize, size_t Count)
	{
		size_t rd = fread(DstBuf, ElementSize, Count, _File);
		if (rd < Count)
			_eof = true;
		return rd;
	}

	FILE * _File;
};

#endif // !defined(FILE_H)
