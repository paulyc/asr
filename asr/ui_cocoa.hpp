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
	virtual void main_loop() { throw string_exception("Not implemented. Use AppKit loop"); }
	virtual bool running() { return true; }
	virtual void do_quit() { _quit.signal(); }
	virtual void render(int trackid);
    void render_impl(int trackid);
	virtual void set_track_filename(int t) {}
	virtual void set_position(void *t, double tm, bool invalidate);
	virtual void set_clip(int) {}
	virtual bool want_render() { return false; }
	virtual void set_text_field(int id, const char *txt, bool del) {}
    
    virtual void mouse_down(MouseButton b, int x, int y, int trackid);
	virtual void mouse_up(MouseButton b, int x, int y, int trackid);
	virtual void mouse_dblclick(MouseButton b, int x, int y, int trackid);
	virtual void mouse_drag(int x, int y, int trackid);
    void mouse_scroll(int dy, int trackid);
private:
    Lock_T _lock;
    Condition_T _quit;
};

#endif // !defined(_ASR_UI_COCOA_HPP_)
