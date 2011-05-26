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
		_wav_heights(0),
		_center(0.5),
		_zoom(100000.0)
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

  // fix file not expected length!!
  void set_wav_heights(pthread_mutex_t *lock=0)
  {
	  SamplePairf *s;
	  int chunks = _src->len().chunks;
	  double left = _center - 0.5/_zoom;
	  double right = _center + 0.5/_zoom;
	  int left_chunk = left * chunks;
	  int right_chunk = right * chunks;
	  chunks = right_chunk-left_chunk;
	  //chunk_t *chk = _src->get_chunk(0);
	 // int chunks_per_pixel = (right_chunk-left_chunk) / _width;
	  int chunks_per_pixel = chunks / _width;
	  double chunks_per_pixel_d = double(chunks) / _width;
	  if (chunks_per_pixel_d > 2.0)
	  {
		  int chunks_per_pixel_rem = chunks % _width;
		  bool repeat = chunks_per_pixel_rem > 0;
		  if (repeat)
			  ++chunks_per_pixel;
		  int chk = 0, end_chk;
		  for (int p = 0; p < _width; ++p)
		  {
			  if (lock)
				  pthread_mutex_lock(lock);
			  _wav_heights[p].avg_top = 0.0;
			  for (chk = p*chunks_per_pixel_d, end_chk = chk+chunks_per_pixel; chk < end_chk; ++chk)
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
	  else
	  {
		  chunks = _src->len().chunks;
		  int left_sample = left * chunks * Source_T::chunk_t::chunk_size;
		  int rt_sample = right * chunks * Source_T::chunk_t::chunk_size;
		  int samples_per_pixel = (rt_sample - left_sample) / _width;
		  double samples_per_pixel_d = double(rt_sample - left_sample) / _width;
		  int chk = left_sample / chunks;
		 int ofs = left_sample % chunks;
		 double smp_total = 0.0;
		 //int chk, 
		 for (int p = 0; p < _width; ++p)
		  {
			  if (lock)
				  pthread_mutex_lock(lock);
			  _wav_heights[p].avg_top = 0.0;
			  _wav_heights[p].peak_top = 0.0;
			  int smp = 0;
			  while (smp < samples_per_pixel_d)
			  {
				  Source_T::chunk_t *the_chk = _src->getSrc()->get_chunk(chk);
				  for (; ofs < Source_T::chunk_t::chunk_size && smp < samples_per_pixel_d; ++smp)
				  //for (int end_chk = chk+chunks_per_pixel; chk < end_chk; ++chk)
				  {
					  _wav_heights[p].avg_top += fabs(the_chk->_data[ofs][0]);
					  _wav_heights[p].peak_top = max(fabs(the_chk->_data[ofs][0]), _wav_heights[p].peak_top);
					//  assert(chk < chunks);
				//	const Source_T::ChunkMetadata &meta = _src->get_metadata(chk);
				//	SetMax<double>::calc(_wav_heights[p].peak_top, max(meta.peak[0], meta.peak[1]));
				//	Sum<double>::calc(_wav_heights[p].avg_top, _wav_heights[p].avg_top, max(meta.avg[0], meta.avg[1]));
					  int smp_b = int(smp_total);
					  smp_total += samples_per_pixel_d;
					  if (int(smp_total) > smp_b)
						  ++ofs;
				  }
				  if (smp < samples_per_pixel_d)
				  {
					ofs = 0;
					++chk;
				  }
			  }
			 // Quotient<double>::calc(_wav_heights[p].avg_top, _wav_heights[p].avg_top, chunks_per_pixel);
			  _wav_heights[p].avg_top /= smp;
			//  if (repeat)
			//	  --chk;
			  if (lock)
				  pthread_mutex_unlock(lock);
		  }
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

  void set_ctr(double c)
  {
	  _center = c;
  }

  void set_zoom(double z)
  {
	  if (z >= 1.0)
		_zoom = z;
  }

protected:
	Source_T *_src;
	Controller_T *_controller;
	int _width;
	wav_height *_wav_heights;
	double _center;
	double _zoom;
};

#endif // !defined(_WAVFORMDISPLAY_H)
