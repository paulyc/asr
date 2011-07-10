#include "config.h"
#include "ui.h"

#include <cstdio>

typedef ASIOProcessor ASIOP;
extern ASIOP * asio;
typedef ASIOP::track_t track_t;
extern GenericUI *ui;

GenericUI::GenericUI(ASIOProcessor *io, UITrack t1, UITrack t2) :
	_io(io),
	_track1(t1),
	_track2(t2)
{}

void GenericUI::do_paint()
{
	_io->GetTrack(1)->lockedpaint();
	_io->GetTrack(2)->lockedpaint();
}

void GenericUI::mouse_down(MouseButton b, int x, int y)
{
	switch (b)
	{
		case Left:
		{
			_track1.wave.mousedown = y >= _track1.wave.windowr.top && y < _track1.wave.windowr.bottom &&
				x >= _track1.wave.windowr.left && x < _track1.wave.windowr.right;
			if (_track1.wave.mousedown)
			{
				_lastx=x;
				_lasty=y;
				_io->GetTrack(1)->lock_pos(x-_track1.wave.windowr.left);
			}

			_track2.wave.mousedown = y >= _track2.wave.windowr.top && y < _track2.wave.windowr.bottom &&
				x >= _track2.wave.windowr.left && x < _track2.wave.windowr.right;
			if (_track2.wave.mousedown)
			{
				_lastx=x;
				_lasty=y;
				_io->GetTrack(2)->lock_pos(x-_track2.wave.windowr.left);
			}
			break;
		}
	case Right:
		break;
	case Middle:
		break;
	}
}

void GenericUI::mouse_up(MouseButton b, int x, int y)
{
	switch (b)
	{
		case Left:
		{
			_track1.wave.mousedown = false;
			_track2.wave.mousedown = false;
			_io->GetTrack(1)->unlock_pos();
			_io->GetTrack(2)->unlock_pos();
			break;
		}
		case Right:
		{
			if (y > _track1.wave.windowr.top && y < _track1.wave.windowr.bottom)
			{
				double f = double(x - _track1.wave.windowr.left)/_track1.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("cue %f\n", f);
					_io->GetTrack(1)->set_cuepoint(_io->GetTrack(1)->get_display_time(f));
				}
			}
			else if (y > _track2.wave.windowr.top && y < _track2.wave.windowr.bottom)
			{
				double f = double(x - _track2.wave.windowr.left)/_track2.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("cue %f\n", f);
					_io->GetTrack(2)->set_cuepoint(_io->GetTrack(2)->get_display_time(f));
				}
			}
			break;
		}
	case Middle:
		break;
	}
}

void GenericUI::mouse_dblclick(MouseButton b, int x, int y)
{
	switch (b)
	{
		case Left:
		{
			if (y >= _track1.wave.windowr.top && y < _track1.wave.windowr.bottom)
			{
				double f = double(x - _track1.wave.windowr.left)/_track1.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("%f\n", f);
					_io->GetTrack(1)->seek_time(_io->GetTrack(1)->get_display_time(f));
				}
			}
			else if (y >= _track2.wave.windowr.top && y < _track2.wave.windowr.bottom)
			{
				double f = double(x - _track2.wave.windowr.left)/_track2.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("%f\n", f);
					_io->GetTrack(2)->seek_time(_io->GetTrack(2)->get_display_time(f));
				}
			}
			break;
		}
	case Right:
		break;
	case Middle:
		break;
	}
}

void GenericUI::mouse_move(int x, int y)
{
	int dx = x-_lastx;
	int dy = y-_lasty;

	if (_track1.wave.mousedown)
	{
		if (dy)
		{
			_io->GetTrack(1)->zoom_px(dy);
		}
		if (dx)
		{
			_io->GetTrack(1)->move_px(dx);
			_io->GetTrack(1)->lock_pos(x-_track1.wave.windowr.left);
		}
		_lastx = x;
		_lasty = y;
	}
	else if (_track2.wave.mousedown)
	{
		if (dy)
		{
			_io->GetTrack(2)->zoom_px(dy);
		}
		if (dx)
		{
			_io->GetTrack(2)->move_px(dx);
			_io->GetTrack(2)->lock_pos(x-_track2.wave.windowr.left);
		}
		_lastx = x;
		_lasty = y;
	}
}

void GenericUI::slider_move(double pos, SliderType t, int x)
{
	switch (t)
	{
		case PitchCoarse:
			break;
		case PitchFine:
			break;
		case Gain:
		{
			(x==1?_track1:_track2).gain.set_text_db(20.0*log(1.25*pos));
			_io->set_gain(x, 1.25*pos);
			break;
		}
	}
}

UITrack::UITrack(GenericUI *ui, int tid, int pitch_id, int gain_id) :
	id(tid),
	coarse_val(48000.0),
	fine_val(0.0),
	pitch(ui, pitch_id),
	gain(ui, gain_id)
{}

void UITrack::set_coarse(double v)
{
	coarse_val = 48000.0 / (1.0 + .4 * v -0.2);
//	printf("coarse_val %f\n", _track2.coarse_val);
	double val = 48000.0 / (coarse_val+fine_val) - 1.0;
	printf("track %d: %f\n", id, val*100.0);
	pitch.set_text_pct(val*100.0);
	asio->GetTrack(id)->set_output_sampling_frequency(coarse_val+fine_val); 
}

void UITrack::set_fine(double v)
{
	fine_val = 800.0 -  1600.*v;
//	printf("fine_val %f\n", fine_val);
	double val = 48000.0 / (coarse_val+fine_val) - 1.0;
	printf("track %d: %f\n", id, val*100.0);
	pitch.set_text_pct(val*100.0);
	asio->GetTrack(id)->set_output_sampling_frequency(coarse_val+fine_val); 
}

UIText::UIText(GenericUI *ui, int i) :
	callback(ui, &GenericUI::set_text_field),
	id(i)
{
}

void UIText::set_text(const wchar_t *txt)
{
	callback(id, txt);
}

void UIText::set_text_pct(double pct)
{
	wchar_t buf[64];
	swprintf(buf, L"%.02f%%", pct);
	set_text(buf);
}

void UIText::set_text_db(double db)
{
	wchar_t buf[64];
	swprintf(buf, L"%.02f dB", db);
	set_text(buf);
}
