// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef _ASR_UI_COCOA_HPP_
#define _ASR_UI_COCOA_HPP_

class CocoaUI : public GenericUI
{
public:
    CocoaUI() : GenericUI(0,
                          UITrack(this, 1, 1, 2, 3),
                          UITrack(this, 2, 4, 5, 6)) {}
    virtual void create() {}
	virtual void destroy() {}
	virtual void main_loop() { throw std::runtime_error("Not implemented. Use AppKit loop"); }
	virtual bool running() { return true; }
	virtual void do_quit() { _quit.signal(); }
	virtual void render(int trackid);
    void render_impl(int trackid);
	virtual void set_track_filename(int t) {}
	virtual void set_position(void *t, double tm, bool invalidate);
	virtual void set_clip(int) {}
	virtual bool want_render() { return true; }
	virtual void set_text_field(int id, const char *txt, bool del) {}
    
    virtual void mouse_down(MouseButton b, int x, int y, int trackid);
	virtual void mouse_up(MouseButton b, int x, int y, int trackid);
	virtual void mouse_dblclick(MouseButton b, int x, int y, int trackid);
	virtual void mouse_drag(int x, int y, int trackid);
    void mouse_scroll(int dx, int dy, int x, int trackid);
    virtual void play_pause(int trackid);
protected:
    Lock_T _lock;
    Condition_T _quit;
};

#endif // !defined(_ASR_UI_COCOA_HPP_)
