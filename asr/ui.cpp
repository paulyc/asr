#include "config.h"
#include "ui.h"

#include <cstdio>

typedef ASIOProcessor ASIOP;
//extern ASIOP * asio;
typedef ASIOP::track_t track_t;
//extern GenericUI *ui;

void MagicController::do_magic()
{
	if (_magic_count == -1)
		_magic_count = 0;
	else
		_magic_count = -1; // disable if hit button again before finishing
}
/*
void MagicController::do_magic()
{
	if (_magic_count++ == 3)
		_magic_count = -1;
	wchar_t buf[64];
	if (_magic_count == -1)
		wsprintf(buf, L"M = ");
	else
		wsprintf(buf, L"M%d = %f.3", _magic_count, _magic_times[_magic_count]);
	_io->get_ui()->set_text_field(1069, buf);
}
*/
void MagicController::next_time(double t, int t_id)
{
	switch (_magic_count)
	{
	case -1:
		break;
	case 0:
		_first_track = t_id;
		_magic_times[_magic_count++] = t;
		break;
	case 1:
		if (t_id == _first_track)
		{
			_magic_times[_magic_count++] = t;
		}
		break;
	case 2:
		if (t_id != _first_track)
		{
			_second_track = t_id;
			_magic_times[_magic_count++] = t;
		}
		break;
	case 3:
		if (t_id == _second_track)
		{
			const double dt1 = _magic_times[1] - _magic_times[0];
			const double dt2 = t - _magic_times[2];
			const double ratio = dt2 / dt1;
			const double pitch_other = _io->GetTrack(_first_track)->get_pitch();
			_io->GetTrack(t_id)->set_pitch(ratio * pitch_other);
			_magic_count = -1;
		}
		break;
	default:
		assert("should not happen" && false);
		break;
	}
	/*
	switch (_magic_count)
	{
	case -1:
		break;
	case 0:
	case 1:
		_magic_times[_magic_count] = t;
		_first_track = t_id;
		_second_track = t_id == 1 ? 2 : 1;
		break;
	case 2:
		if (t_id == _second_track)
			_magic_times[_magic_count] = t;
		break;
	case 3:
		if (t_id == _second_track)
		{
			_magic_times[_magic_count] = t;
			const double dt1 = _magic_times[1] - _magic_times[0];
			const double dt2 = t - _magic_times[2];
			const double ratio = dt2 / dt1;
			const double pitch_other = _io->GetTrack(_first_track)->get_pitch();
			_io->GetTrack(_second_track)->set_pitch(ratio * pitch_other);
		}
		break;
	}*/
}

GenericUI::GenericUI(ASIOProcessor *io, UITrack t1, UITrack t2) :
	_io(io),
	_track1(t1),
	_track2(t2),
	_magic(io)
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
	bool inWave1=false, inWave2=false;
	double tm;

	if (y > _track1.wave.windowr.top && y < _track1.wave.windowr.bottom)
	{
		double f = double(x - _track1.wave.windowr.left)/_track1.wave.width();
		if (f >= 0.0 && f <= 1.0)
		{
			inWave1 = true;
			tm = _io->GetTrack(1)->get_display_time(f);
		}
	}
	else if (y > _track2.wave.windowr.top && y < _track2.wave.windowr.bottom)
	{
		double f = double(x - _track2.wave.windowr.left)/_track2.wave.width();
		if (f >= 0.0 && f <= 1.0)
		{
			inWave2 = true;
			tm = _io->GetTrack(2)->get_display_time(f);
		}
	}

	switch (b)
	{
		case Left:
		{
			if (inWave1)
			{
				_magic.next_time(tm, 1);
			}
			else if (inWave2)
			{
				_magic.next_time(tm, 2);
			}

			_track1.wave.mousedown = false;
			_track2.wave.mousedown = false;
			_io->GetTrack(1)->unlock_pos();
			_io->GetTrack(2)->unlock_pos();
			break;
		}
		case Right:
		{
			if (inWave1)
			{
				_io->GetTrack(1)->set_cuepoint(tm);
			}
			else if (inWave2)
			{
				_io->GetTrack(2)->set_cuepoint(tm);
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

void GenericUI::render_dirty()
{
    if (_track1.dirty)
		_io->GetTrack(1)->draw_if_loaded();
    if (_track2.dirty)
        _io->GetTrack(2)->draw_if_loaded();
}

UITrack::UITrack(GenericUI *ui, int tid, int pitch_id, int gain_id) :
	id(tid),
	coarse_val(48000.0),
	fine_val(0.0),
	pitch(ui, pitch_id),
	gain(ui, gain_id),
	clip(false),
    dirty(false)
{}

void UITrack::set_coarse(double v)
{
	coarse_val = 48000.0 / (1.0 + .4 * v -0.2);
//	printf("coarse_val %f\n", _track2.coarse_val);
	double val = 48000.0 / (coarse_val+fine_val) - 1.0;
	printf("track %d: %f\n", id, val*100.0);
	pitch.set_text_pct(val*100.0);
	ASR::get_io_instance()->GetTrack(id)->set_output_sampling_frequency(coarse_val+fine_val); 
}

void UITrack::set_fine(double v)
{
	fine_val = 800.0 -  1600.*v;
//	printf("fine_val %f\n", fine_val);
	double val = 48000.0 / (coarse_val+fine_val) - 1.0;
	printf("track %d: %f\n", id, val*100.0);
	pitch.set_text_pct(val*100.0);
	ASR::get_io_instance()->GetTrack(id)->set_output_sampling_frequency(coarse_val+fine_val); 
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
