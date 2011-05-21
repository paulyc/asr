#ifndef _WAVFORMDISPLAY_H
#define _WAVFORMDISPLAY_H

template <Source_T, Controller_T>
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
		  wav_height() : top(0.0), bottom(0.0){}
		  double top;
		  double bottom;
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

protected:
	Source_T *_src;
	Controller_T *_controller;
	int _width;
	wav_height *_wav_heights;
};

#endif // !defined(_WAVFORMDISPLAY_H)
