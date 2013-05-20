//
//  MainView.m
//  mac
//
//  Created by Paul Ciarlo on 5/17/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

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
    return NO;
}

- (void)drawRect:(NSRect)dirtyRect
{
    // Drawing code here.
    [super drawRect:dirtyRect];
}


- (void)keyUp:(NSEvent *)theEvent
{
    AppDelegate *del = [NSApp delegate];
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
