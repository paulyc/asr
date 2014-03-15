// ASR - Digital Signal Processor
// Copyright (C) 2002-2013	Paul Ciarlo
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

#include "ui.h"

#if IOS
#include <UIKit/UIKit.h>
#else
#include <AppKit/AppKit.h>
#include "../mac/ASR/AppDelegate.h"
#include "../mac/ASR/MyOpenGLView.h"
#include <GLUT/GLUT.h>
#endif

#include <cstdio>

#include "track.h"

#if !IOS
bool FileOpenDialog::OpenSingleFile(std::string &filename)
{
	NSOpenPanel* openDlg = [NSOpenPanel openPanel];
	
	[openDlg setCanChooseFiles:YES];
	[openDlg setCanChooseDirectories:NO];
	[openDlg setAllowsMultipleSelection:NO];
	
	if ([openDlg runModal] == NSOKButton)
	{
		NSArray* files = [openDlg URLs];
		
		for( int i = 0; i < [files count]; i++ )
		{
			NSURL* nsFileUrl = [files objectAtIndex:i];
			NSLog(@"%@", nsFileUrl.path);
			filename = std::string([nsFileUrl.path UTF8String]);
			return true;
		}
	}
	return false;
}
#endif // !IOS

CocoaUI::CocoaUI() : GenericUI(0,
					  UITrack(this, 1, 1, 2, 3),
					  UITrack(this, 2, 4, 5, 6))
{
	AppDelegate *del = [NSApp delegate];
	MyOpenGLView *glView = [[del.window contentView] viewWithTag:'wav1'];
	[glView setupDragAndDrop];
	glView = [[del.window contentView] viewWithTag:'wav2'];
	[glView setupDragAndDrop];
}

void CocoaUI::mouse_down(MouseButton b, int x, int y, int trackid)
{
	switch (b)
	{
		case Left:
		{
			if (trackid == 1)
			{
				_io->GetTrack(trackid)->lock_pos(x);
				_track1.wave.mousedown = true;
				_lastx = x;
				_lasty = y;
			}
			else if (trackid == 2)
			{
				_io->GetTrack(trackid)->lock_pos(x);
				_track2.wave.mousedown = false;
				_lastx = x;
				_lasty = y;
			}
			break;
		}
		case Right:
			break;
		case Middle:
			break;
	}
}

void CocoaUI::mouse_up(MouseButton b, int x, int y, int trackid)
{
	bool inWave1=trackid==1, inWave2=trackid==2;
	double tm;
	
	if (inWave1)
	{
		double f = double(x)/_track1.wave.width();
		if (f >= 0.0 && f <= 1.0)
		{
			tm = _io->GetTrack(trackid)->get_display_time(f);
			if (b == Right)
			{
				_io->GetTrack(trackid)->set_cuepoint(tm);
			}
		}
	}
	else if (inWave2)
	{
		double f = double(x)/_track1.wave.width();
		if (f >= 0.0 && f <= 1.0)
		{
			tm = _io->GetTrack(trackid)->get_display_time(f);
			if (b == Right)
			{
				_io->GetTrack(trackid)->set_cuepoint(tm);
			}
		}
	}
	
	if (b == Left)
	{
		_track1.wave.mousedown = false;
		_track2.wave.mousedown = false;
		_io->GetTrack(1)->unlock_pos();
		_io->GetTrack(2)->unlock_pos();
	}
}

void CocoaUI::mouse_dblclick(MouseButton b, int x, int y, int trackid)
{
	switch (b)
	{
		case Left:
		{
			if (trackid == 1)
			{
				double f = double(x)/_track1.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("%f\n", f);
					_io->GetTrack(trackid)->seek_time(_io->GetTrack(trackid)->get_display_time(f));
				}
			}
			else if (trackid == 2)
			{
				double f = double(x)/_track2.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("%f\n", f);
					_io->GetTrack(trackid)->seek_time(_io->GetTrack(trackid)->get_display_time(f));
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

void CocoaUI::mouse_drag(int x, int y, int trackid)
{
	if (trackid != 1 && trackid != 2)
		return;
	
	const int dx = x-_lastx;
	const int dy = _lasty - y;
	
	if (dy)
	{
		_io->GetTrack(trackid)->zoom_px(dy);
	}
	if (dx)
	{
		_io->GetTrack(trackid)->move_px(dx);
		_io->GetTrack(trackid)->lock_pos(x);
	}
	
	_lastx = x;
	_lasty = y;
}

void CocoaUI::mouse_scroll(int dx, int dy, int x, int trackid)
{
	if (trackid == 1 || trackid == 2)
	{
		if (dy)
		{
			_io->GetTrack(trackid)->lock_pos(x);
			_io->GetTrack(trackid)->zoom_px(-dy);
			_io->GetTrack(trackid)->unlock_pos();
		}
		if (dx)
		{
			_io->GetTrack(trackid)->move_px(5*dx);
		}
	}
}

void CocoaUI::set_position(void *t, double p, bool invalidate)
{
	if (_io)
	{
		UITrack *uit = (t == _io->GetTrack(1) ? &_track1 : &_track2);
		uit->wave.playback_pos = p;
	}
}

void CocoaUI::play_pause(int trackid)
{
	_io->GetTrack(trackid)->play_pause();
}

#if !IOS
void CocoaUI::render(int trackid)
{
	AppDelegate *del = [NSApp delegate];
	if (trackid==1)
		[del performSelectorOnMainThread:@selector(renderTrack:) withObject:@"1" waitUntilDone:NO];
	else
		[del performSelectorOnMainThread:@selector(renderTrack:) withObject:@"2" waitUntilDone:NO];
}

void CocoaUI::render_impl(int trackid)
{
	AppDelegate *del = [NSApp delegate];
	MyOpenGLView *glView;
	if (trackid==1)
		glView = [[del.window contentView] viewWithTag:'wav1'];
	else
		glView = [[del.window contentView] viewWithTag:'wav2'];
	
	[glView renderInto];
	
	const NSRect rect = [glView bounds];
	const int width = rect.size.width;
	const double unit = 1.0 / width;
	
	IOProcessor::track_t *track = _io->GetTrack(trackid);
	UITrack *uit;
	if (trackid==1)
		uit = &_track1;
	else
		uit = &_track2;
	
	if (!track->loaded())
		return;
	
	if (width != track->get_display_width())
	{
		track->set_display_width(width);
		track->set_wav_heights(false, false);
		uit->wave.windowr.left = rect.origin.x;
		uit->wave.windowr.right = rect.origin.x + rect.size.width;
		uit->wave.windowr.bottom = rect.origin.y;
		uit->wave.windowr.top = rect.origin.y + rect.size.height;
		uit->wave.r.left = 0;
		uit->wave.r.right = rect.size.width;
		uit->wave.r.bottom = 0;
		uit->wave.r.top = rect.size.height;
	}
	
	glLoadIdentity();
	glOrtho(0.0, 1.0, 1.0, 0.0, 100000, -100000);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBegin(GL_LINES);
	
	double x = unit / 2.0;
	for (int p=0; p<width; ++p)
	{
		const IOProcessor::track_t::display_t::wav_height &h =
		track->get_wav_height(p);
		double px_pk = 0.5-h.peak_top/2;
		double px_avg_top = 0.5-h.avg_top/2;
		double px_avg_bot = 0.5+h.avg_top/2;
		double px_mn = 0.5+fabs(h.peak_bot)/2;
		
		if (h.peak_top != 0.0)
		{
			glColor3f(0.0f, 0.7f, 0.0f);
			glVertex2d(x, px_pk);
			glVertex2d(x, px_avg_top);
			if (h.peak_bot != 0.0)
			{
				glColor3f(0.0f, 1.0f, 0.0f);
				glVertex2d(x, px_avg_top);
				glVertex2d(x, px_avg_bot);
				glColor3f(0.0f, 0.7f, 0.0f);
				glVertex2d(x, px_avg_bot);
				glVertex2d(x, px_mn);
			}
			else
			{
				glColor3f(0.0f, 0.7f, 0.0f);
				glVertex2d(x, 0.5);
				glVertex2d(x, 0.5-h.peak_top/2);
			}
		}
		else
		{
			glColor3f(0.0f, 0.7f, 0.0f);
			glVertex2d(x, 0.5);
			glVertex2d(x, 0.5+fabs(h.peak_bot)/2);
		}
		x += unit;
	}
	
   // double len = track->len().time;
	track->update_position();
	double pos = uit->wave.playback_pos;
	if (pos >= 0.0 && pos <= 1.0)
	{
		x = pos;
		glColor3f(1.0f, 1.0f, 0.0f);
		glVertex2d(x, 0.0);
		glVertex2d(x, 1.0);
	}
	
	x = track->get_cuepoint_pos();
	glColor3f(1.0f, 0.5f, 0.0f);
	glVertex2d(x, 0.0);
	glVertex2d(x, 1.0);
	
	
#define SHOW_BEATS 1
#if SHOW_BEATS
	glColor3f(1.0f, 0.0f, 0.5f);
	const std::vector<double>& beats = track->beats();
	for (int i = 0; i < beats.size(); ++i)
	{
		double pos = track->get_display_pos(beats[i]);
		if (pos < 0.0)
			continue;
		else if (pos > 1.0)
			break;
		else
		{
			glVertex2d(pos, 0.0);
			glVertex2d(pos, 1.0);
		}
	}
#endif
	glEnd();
	
	glFlush();
	glSwapAPPLE();
}
#endif // !IOS
