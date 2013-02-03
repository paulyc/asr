#include "io.h"
#include "controller.h"

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
			if (t_id == 1)
				_io->get_ui()->_track1.update_frequency(48000.0/(ratio * pitch_other));
			else
				_io->get_ui()->_track2.update_frequency(48000.0/(ratio * pitch_other));
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
