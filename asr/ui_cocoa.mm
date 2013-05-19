#include "ui.h"

#include <AppKit/AppKit.h>
#include <cstdio>

#include "../mac/ASR/AppDelegate.h"
#include "../mac/ASR/MyOpenGLView.h"
#include "track.h"

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
    
    ASIOProcessor::track_t *track = _io->GetTrack(trackid);
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
	}
    
    
    
    glOrtho(0.0, 1.0, 1.0, 0.0, 100000, -100000);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_LINES);
    
    double x = unit / 2.0;
    for (int p=0; p<width; ++p)
	{
		const ASIOProcessor::track_t::display_t::wav_height &h =
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
                glColor3f(0.0f, 1.0f, 0.0f);
                glVertex2d(x, px_avg_top);
                glVertex2d(x, px_avg_bot);
			}
		}
		else
		{
            glColor3f(0.0f, 1.0f, 0.0f);
            glVertex2d(x, px_avg_top);
            glVertex2d(x, px_avg_bot);
            if (h.peak_bot != 0.0)
            {
                glColor3f(0.0f, 0.7f, 0.0f);
                glVertex2d(x, px_avg_bot);
                glVertex2d(x, px_mn);
            }
		}
		x += unit;
	}
    
    double len = track->len().time;
	//px = int(playback_pos / len * double(width));
	if (uit->wave.playback_pos >= 0.0 && uit->wave.playback_pos <= 1.0)
	{
		uit->wave.px = uit->wave.playback_pos;
        glColor3f(1.0f, 1.0f, 0.0f);
        glVertex2d(uit->wave.px, 0.0);
        glVertex2d(uit->wave.px, 1.0);
	}
    
	uit->wave.cpx = track->get_cuepoint_pos();
    glColor3f(1.0f, 0.5f, 0.0f);
    glVertex2d(uit->wave.cpx, 0.0);
    glVertex2d(uit->wave.cpx, 1.0);
    glEnd();
    
#define SHOW_BEATS 0
#if SHOW_BEATS
	const std::vector<double>& beats = track->beats();
	for (int i = 0; i < beats.size(); ++i)
	{
		double pos = track->get_display_pos(beats[i]);
		if (pos >= 0.0 && pos <= 1.0)
		{
            glColor3f(1.0f, 0.0f, 0.5f);
            glVertex2d(pos, 0.0);
            glVertex2d(pos, 1.0);
		}
	}
#endif
    
    glFlush();
    glSwapAPPLE();
}
