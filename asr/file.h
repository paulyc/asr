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
	DiskFile(const wchar_t *filename, const wchar_t *mode=L"rb")
	{
		_File = _wfopen(filename, mode);
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
