#ifndef _WAVFORMDISPLAY_H
#define _WAVFORMDISPLAY_H

#include <pthread.h>
#include "type.h"
#include "ui.h"

template <typename Source_T, typename Controller_T>
class WavFormDisplay
{
public:
	WavFormDisplay(Source_T *src, Controller_T *controller, int width=0) :
		_src(src),
		_controller(controller),
		_wav_heights(0),
		_center(0.5),
		_zoom(1.0),
		_indx(0),
		_left(0.0),
		_right(1.0),
		_pos_locked(false)
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
	_indx = 0;
  }

  int get_width()
  {
	  return _width;
  }
  
	bool set_next_height()
	{
		if (_indx >= _width)
			return false;

		int chunks = _src->len().chunks;
		int left_chunk = _left * chunks;
		int right_chunk = _right * chunks;
		chunks = right_chunk-left_chunk;

		int chunks_per_pixel = chunks / _width;
	  double chunks_per_pixel_d = double(chunks) / _width;
	  if (chunks_per_pixel_d > 2.0)
	  {
		  int chunks_per_pixel_rem = chunks % _width;
		  bool repeat = chunks_per_pixel_rem > 0;
		  if (repeat)
			  ++chunks_per_pixel;
		  int chk = 0, end_chk;
		  wav_height &h = _wav_heights[_indx];
		//  for (int p = 0; p < _width; ++p)
		  {
//			  if (lock)
//				  pthread_mutex_lock(lock);
			  h.avg_top = 0.0;
			  h.peak_top = 0.0;
			  for (chk = _indx*chunks_per_pixel_d, end_chk = chk+chunks_per_pixel; chk < end_chk; ++chk)
			  {
				  assert(chk < chunks);
				const Source_T::ChunkMetadata &meta = _src->get_metadata(chk);
				SetMax<double>::calc(h.peak_top, max(meta.peak[0], meta.peak[1]));
				Sum<double>::calc(h.avg_top, h.avg_top, max(meta.avg[0], meta.avg[1]));
			  }
			  Quotient<double>::calc(h.avg_top, h.avg_top, chunks_per_pixel);
			  if (repeat)
				  --chk;
//			  if (lock)
//				  pthread_mutex_unlock(lock);
		  }
	  }
	  else
	  {
		  chunks = _src->len().chunks;
		  int left_sample = _left * chunks * Source_T::chunk_t::chunk_size;
		  int rt_sample = _right * chunks * Source_T::chunk_t::chunk_size;
		  int samples_per_pixel = (rt_sample - left_sample) / _width;
		  double samples_per_pixel_d = double(rt_sample - left_sample) / _width;
		  int chk = left_sample / chunks;
		 int ofs = left_sample % chunks;
		 double smp_total = 0.0;
		 //int chk, 
		 wav_height &h = _wav_heights[_indx];
	//	 for (int p = 0; p < _width; ++p)
		  {
//			  if (lock)
//				  pthread_mutex_lock(lock);
			  h.avg_top = 0.0;
			  h.peak_top = 0.0;
			  int smp = 0;
			  while (smp < samples_per_pixel_d)
			  {
				  Source_T::chunk_t *the_chk = _src->getSrc()->get_chunk(chk);
				  for (; ofs < Source_T::chunk_t::chunk_size && smp < samples_per_pixel_d; ++smp)
				  //for (int end_chk = chk+chunks_per_pixel; chk < end_chk; ++chk)
				  {
					  h.avg_top += fabs(the_chk->_data[ofs][0]);
					  h.peak_top = max(fabs(the_chk->_data[ofs][0]), h.peak_top);
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
			  h.avg_top /= smp;
			//  if (repeat)
			//	  --chk;
//			  if (lock)
//				  pthread_mutex_unlock(lock);
		  }
	  }

		++_indx;
		return true;
  }
#if 0
	void set_wav_heights(pthread_mutex_t *lock=0)
	{
		if (lock)
			pthread_mutex_lock(lock);
		_indx=0;
		while (set_next_height())
			;
		if (lock)
			pthread_mutex_unlock(lock);
	}
#else
  // fix file not expected length!!
  void set_wav_heights(pthread_mutex_t *lock=0)
  {
	  SamplePairf *s;
	  int chunks_total = _src->len().chunks;
	  int left_chunk = _left * chunks_total;
	  int right_chunk = _right * chunks_total;
	  int chunks = right_chunk-left_chunk;
	  //chunk_t *chk = _src->get_chunk(0);
	 // int chunks_per_pixel = (right_chunk-left_chunk) / _width;
	//  int chunks_per_pixel = chunks / _width;
	  double chunks_per_pixel_d = double(chunks) / _width;
	  int chunks_per_pixel = max(1, int(chunks_per_pixel_d));
	  if (chunks_per_pixel_d > 1.0)
	  {
		  
		  for (int p = 0; p < _width; ++p)
		  {
			  int chk = left_chunk + p*chunks_per_pixel_d;
			  if (lock)
				  pthread_mutex_lock(lock);
			  _wav_heights[p].peak_top = 0.0;
			  _wav_heights[p].avg_top = 0.0;
			  for (int end_chk = chk+chunks_per_pixel; chk < end_chk; ++chk)
			  {
				  assert(chk < chunks_total);
				const Source_T::ChunkMetadata &meta = _src->get_metadata(chk);
				SetMax<double>::calc(_wav_heights[p].peak_top, max(meta.peak[0], meta.peak[1]));
				Sum<double>::calc(_wav_heights[p].avg_top, _wav_heights[p].avg_top, max(meta.avg[0], meta.avg[1]));
			  }
			  Quotient<double>::calc(_wav_heights[p].avg_top, _wav_heights[p].avg_top, chunks_per_pixel);
			//  if (repeat)
			//	  --chk;
			  if (lock)
				  pthread_mutex_unlock(lock);
		  }
	  }
	  else
	  {
		  chunks = _src->len().chunks;
		  int left_sample = _left * chunks * Source_T::chunk_t::chunk_size;
		  int rt_sample = _right * chunks * Source_T::chunk_t::chunk_size;
		  int samples_per_pixel = (rt_sample - left_sample) / _width;
		  double samples_per_pixel_d = double(rt_sample - left_sample) / _width;
		  double pixels_per_sample_d = 1.0/samples_per_pixel_d;
		  int chk = left_sample / chunks;
		 int ofs = left_sample % chunks, num_smp = max(1,int(samples_per_pixel_d));
		 double smp_total = 0.0;
		 //int chk, 
		 for (int p = 0; p < _width; ++p)
		  {
			  double smp_l = left_sample + p*samples_per_pixel_d;
			  chk = int(smp_l) / Source_T::chunk_t::chunk_size;
			  ofs = int(smp_l) % Source_T::chunk_t::chunk_size;
			  
			  if (lock)
				  pthread_mutex_lock(lock);
			  _wav_heights[p].avg_top = 0.0;
			  _wav_heights[p].peak_top = 0.0;
			  int smp = 0;
			  while (smp < num_smp)
			  {
				  Source_T::chunk_t *the_chk = _src->getSrc()->get_chunk(chk);
				  for (; ofs < Source_T::chunk_t::chunk_size && smp < num_smp; ++ofs, ++smp)
				  {
					  _wav_heights[p].avg_top += fabs(the_chk->_data[ofs][0]);
					  _wav_heights[p].peak_top = max(fabs(the_chk->_data[ofs][0]), _wav_heights[p].peak_top);
					//  assert(chk < chunks);
				//	const Source_T::ChunkMetadata &meta = _src->get_metadata(chk);
				//	SetMax<double>::calc(_wav_heights[p].peak_top, max(meta.peak[0], meta.peak[1]));
				//	Sum<double>::calc(_wav_heights[p].avg_top, _wav_heights[p].avg_top, max(meta.avg[0], meta.avg[1]));
				  }
				  if (smp < num_smp)
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
#endif
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
	  if (c - 0.5/_zoom >= 0.0 && c + 0.5/_zoom <= 1.0)
	  {
			_center = c;
		  //else
		//	  _center = 1.0;
		  _indx = 0;
		  _left = _center - 0.5/_zoom;
		  _right = _center + 0.5/_zoom;
	  }
  }

  void recenter()
  {
	  if (_center - 0.5/_zoom < 0.0)
	  {
		  _center = 0.5/_zoom;
	  }
	  else if (_center + 0.5/_zoom > 1.0)
	  {
		  _center = 1.0 - 0.5/_zoom;
	  }
  }

  void set_zoom(double z)
  {
	  double oldz = _zoom;
	  if (z >= 1.0)
		_zoom = z;
	  else
		  _zoom=1.0;
	  if (_pos_locked)
	  {
		  double oldp = (_right - _left) * (double(_lock_px)/_width);
		  double oldpz = oldp * oldz;
		  double oldpzw = oldpz /_zoom;
		//  double newwidth = 1.0/_zoom;
		  double newleft = _left + oldp - oldpzw;
		  double newright = newleft + 1.0/_zoom;
		  _center = newleft + 0.5/_zoom;
	  }
	  recenter();
	  _indx = 0;
		_left = _center - 0.5/_zoom;
		_right = _center + 0.5/_zoom;
	  printf("z %f\n",_zoom);
  }

	double get_display_time(double f)
	{
		return (_left+f*(_right-_left))*_src->len().time;
	}

	double get_display_pos(double tm)
	{
		double d = tm/_src->len().time;
		return (d - _left)/(_right-_left);
	}

  void zoom_px(int dz)
  {
	  if (dz>0)
		set_zoom(pow(0.97,dz)*_zoom);
	  else
		  set_zoom(pow(1.03,-dz)*_zoom);
  }

  void move_px(int dz)
  {
	  if (dz>0)
		set_ctr(_center - dz*(_right-_left)/_width);
	  else
		  set_ctr(_center -dz*(_right-_left)/_width);
  }

  void lock_pos(int y)
  {
	  _lock_px = y;
	  _pos_locked = true;
  }

  void unlock_pos()
  {
	  _pos_locked = false;
  }

protected:
	Source_T *_src;
	Controller_T *_controller;
	int _width;
	wav_height *_wav_heights;
	double _center;
	double _zoom;
	int _indx;
	double _left;
	double _right;
	bool _pos_locked;
	int _lock_px;
};

#endif // !defined(_WAVFORMDISPLAY_H)
