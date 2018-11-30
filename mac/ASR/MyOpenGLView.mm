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

#import "MyOpenGLView.h"
#import "AppDelegate.h"
#import "../../asr/ui.h"
#import "../../asr/track.h"

#include <GLUT/GLUT.h>

@implementation MyOpenGLView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code here.
    }
    
    return self;
}

- (void)setupDragAndDrop
{
	[self registerForDraggedTypes:[NSArray arrayWithObjects:
								   NSURLPboardType, nil]];
}

- (NSInteger)tag
{
    if ([self.identifier isEqualToString:@"Wave1"])
        return 'wav1';
    else
        return 'wav2';
}

- (void)renderInto
{
    [[self openGLContext] makeCurrentContext];
    //[self registerForDraggedTypes:[NSArray arrayWithObjects:NSURLPboardType, nil]];
}

- (int)trackID
{
    if ([self.identifier isEqualToString:@"Wave1"])
        return 1;
    else
        return 2;
}

- (NSPoint)windowCoordsToViewCoords:(NSPoint)p
{
    NSRect r = [self frame];
    p.x -= r.origin.x;
    p.y -= r.origin.y;
    return p;
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    id delId = [NSApp delegate];
    AppDelegate *del = delId;
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_drag(p.x, p.y, [self trackID]);
}

- (void)mouseDown:(NSEvent *)theEvent
{
    id delId = [NSApp delegate];
    AppDelegate *del = delId;
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_down(GenericUI::Left, p.x, p.y, [self trackID]);
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    id delId = [NSApp delegate];
    AppDelegate *del = delId;
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_down(GenericUI::Right, p.x, p.y, [self trackID]);
}

- (void)mouseUp:(NSEvent *)theEvent
{
    id delId = [NSApp delegate];
    AppDelegate *del = delId;
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_up(GenericUI::Left, p.x, p.y, [self trackID]);
    if ([theEvent clickCount] == 2)
        del.ui->mouse_dblclick(GenericUI::Left, p.x, p.y, [self trackID]);
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    id delId = [NSApp delegate];
    AppDelegate *del = delId;
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_up(GenericUI::Right, p.x, p.y, [self trackID]);
    if ([theEvent clickCount] == 2)
        del.ui->mouse_dblclick(GenericUI::Right, p.x, p.y, [self trackID]);
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    id delId = [NSApp delegate];
    AppDelegate *del = delId;
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_scroll([theEvent deltaX], [theEvent deltaY], p.x, [self trackID]);
}

- (void)keyUp:(NSEvent *)theEvent
{
    id delId = [NSApp delegate];
    AppDelegate *del = delId;
    unsigned short keyCode = [theEvent keyCode];

        switch (keyCode)
        {
            case 49:
                del.ui->play_pause([self trackID]);
                break;
            default:
                break;
        }
}

static void drawAnObject ()
{
    glColor3f(1.0f, 0.85f, 0.35f);
    glBegin(GL_TRIANGLES);
    {
        glVertex3f(  0.0,  0.6, 0.0);
        glVertex3f( -0.2, -0.3, 0.0);
        glVertex3f(  0.2, -0.3 ,0.0);
    }
    glEnd();
}

- (void)drawRect:(NSRect)dirtyRect
{
  /*  if ([self.identifier isEqualToString:@"Wave1"])
        glClearColor(0, 0, 0, 0);
    else
        glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    drawAnObject();
    glFlush();
    glSwapAPPLE();*/
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	return NSDragOperationEvery;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
	return YES;
}

- (BOOL) performDragOperation:(id<NSDraggingInfo>)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];
    id delId = [NSApp delegate];
    AppDelegate *del = delId;
    
    if ( [[pboard types] containsObject:NSURLPboardType] ) {
        NSURL *fileURL = [NSURL URLFromPasteboard:pboard];
        NSView *target = [self hitTest:[sender draggingLocation]];
        if ([target isKindOfClass:[MyOpenGLView class]])
        {
            if ([self tag] == 'wav1')
            {
                del.ui->drop_file([[fileURL path] UTF8String], true);
            }
            else if ([self tag] == 'wav2')
            {
                del.ui->drop_file([[fileURL path] UTF8String], false);
            }
        }
    }
    return YES;
}

@end
