#ifndef _FILE_H
#define _FILE_H

#include <cstdio>

class GenericFile
{
public:
	virtual size_t read(void *DstBuf, size_t ElementSize, size_t Count) = 0;
};

template <typename ElementT>
class TestFile : public GenericFile
{
public:
	TestFile(...) :
		_count(0)
	{
	}
	size_t read(void *DstBuf, size_t ElementSize, size_t Count)
	{
		size_t rd = 0;
		ElementT *buf = (ElementT*)DstBuf;
		for (;rd < Count; ++rd)
			*buf++ = _count++;
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
	size_t read(void *DstBuf, size_t ElementSize, size_t Count)
	{
		return fread(DstBuf, ElementSize, Count, _File);
	}

	FILE * _File;
};

#endif // !defined(FILE_H)
