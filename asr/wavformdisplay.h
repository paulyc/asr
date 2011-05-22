#ifndef _WAVFORMDISPLAY_H
#define _WAVFORMDISPLAY_H

#include <pthread.h>
#include "type.h"

template <typename Source_T, typename Controller_T>
class WavFormDisplay
{
public:
	WavFormDisplay(Source_T *src, Controller_T *controller, int width=100) :
		_src(src),
		_controller(controller),
		_wav_heights(0)
	{
		set_width(width);
	}

	  struct wav_height
	  {
		  wav_height() : 
			peak_top(0.0), avg_top(0.0)
			//,peak_bot(0.0), avg_bot(0.0)
			{}
		  double peak_top;
		  double avg_top;
		//  double peak_bot;
		//  double avg_bot;
	  };

  void set_width(int width)
  {
	  if (_wav_heights)
	  {
		  delete [] _wav_heights;
		  _wav_heights = 0;
	  }
	  _width = width;
	_wav_heights = new wav_height[_width];
  }

  int get_width()
  {
	  return _width;
  }

  void set_wav_heights(pthread_mutex_t *lock=0)
  {
	  SamplePairf *s;
	  int chunks = _src->len().chunks;
	  //chunk_t *chk = _src->get_chunk(0);
	  int chunks_per_pixel = chunks / _width;
	  int chunks_per_pixel_rem = chunks % _width;
	  bool repeat = chunks_per_pixel_rem > 0;
	  if (repeat)
		  ++chunks_per_pixel;
	  int chk = 0;
	  for (int p = 0; p < _width; ++p)
	  {
		  if (lock)
			  pthread_mutex_lock(lock);
		  _wav_heights[p].avg_top = 0.0;
		  for (int end_chk = chk+chunks_per_pixel; chk < end_chk; ++chk)
		  {
			  assert(chk < chunks);
			const Source_T::ChunkMetadata &meta = _src->get_metadata(chk);
			SetMax<double>::calc(_wav_heights[p].peak_top, max(meta.peak[0], meta.peak[1]));
			Sum<double>::calc(_wav_heights[p].avg_top, _wav_heights[p].avg_top, max(meta.avg[0], meta.avg[1]));
		  }
		  Quotient<double>::calc(_wav_heights[p].avg_top, _wav_heights[p].avg_top, chunks_per_pixel);
		  if (repeat)
			  --chk;
		  if (lock)
			  pthread_mutex_unlock(lock);
	  }
  }

  const wav_height& get_wav_height(int pixel)
  {
	  assert(pixel<_width);
	  return _wav_heights[pixel];
  }

  void repaint()
  {
  }

protected:
	Source_T *_src;
	Controller_T *_controller;
	int _width;
	wav_height *_wav_heights;
};

#endif // !defined(_WAVFORMDISPLAY_H)
