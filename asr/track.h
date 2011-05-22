#ifndef _TRACK_H
#define _TRACK_H

#include "wavfile.h"

template <Chunk_T>
class Track : public T_source<Chunk_T>
{
public:
	Track() :
	  _src(0)
	{
	}
	void set_source_file(const wchar_t *filename)
	{
		if (_src)
		{
			delete _src;
			_src = 0;
		}
		_src = new wavfile_chunker<Chunk_T>(filename);
	}
protected:
	wavfile_chunker<Chunk_T> *_src;
};

#endif // !defined(TRACK_H)
