// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
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

#import "MainView.h"
#import "AppDelegate.h"
#import "../../asr/ui.h"
#import "../../asr/track.h"
#import "MyOpenGLView.h"

@implementation MainView

+ (void)initialize
{
    [super initialize];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code here.
    }
    
    return self;
}

- (BOOL)acceptsFirstResponder {
    return NO; // change to YES and help magically works
}

- (void)showHelp:(id)sender
{
	printf("Hello");
}

- (void)drawRect:(NSRect)dirtyRect
{
    // Drawing code here.
    [super drawRect:dirtyRect];
}


- (void)keyUp:(NSEvent *)theEvent
{
    id delId = [NSApp delegate];
    AppDelegate *del = delId;
    NSPoint p = [theEvent locationInWindow];
    
    NSView *target = [self hitTest:p];
    /*if ([target isKindOfClass:[MyOpenGLView class]])
    {
        unsigned short keyCode = [theEvent keyCode];
        if ([target tag] == 'wav1')
        {
            switch (keyCode)
            {
                case 49:
                    del.ui->_io->GetTrack(1)->play_pause();
                    break;
                default:
                    break;
            }
        }
        else if ([target tag] == 'wav2')
        {
            switch (keyCode)
            {
                case 49:
                    del.ui->_io->GetTrack(2)->play_pause();
                    break;
                default:
                    break;
         
     }
        }
    }*/
    unsigned short keyCode = [theEvent keyCode];
    switch (keyCode)
    {
        case 49:
            del.ui->_io->GetTrack(1)->play_pause();
            break;
        default:
            break;
    }
}

@end
