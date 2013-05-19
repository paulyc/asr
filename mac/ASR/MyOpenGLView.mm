//
//  MyOpenGLView.m
//  mac
//
//  Created by Paul Ciarlo on 5/17/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#import "MyOpenGLView.h"
#import "AppDelegate.h"
#import "../../asr/ui.h"

@implementation MyOpenGLView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code here.
    }
    
    return self;
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
    AppDelegate *del = [NSApp delegate];
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_drag(p.x, p.y, [self trackID]);
}

- (void)mouseDown:(NSEvent *)theEvent
{
    AppDelegate *del = [NSApp delegate];
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_down(GenericUI::Left, p.x, p.y, [self trackID]);
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    AppDelegate *del = [NSApp delegate];
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_down(GenericUI::Right, p.x, p.y, [self trackID]);
}

- (void)mouseUp:(NSEvent *)theEvent
{
    AppDelegate *del = [NSApp delegate];
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_up(GenericUI::Left, p.x, p.y, [self trackID]);
    if ([theEvent clickCount] == 2)
        del.ui->mouse_dblclick(GenericUI::Left, p.x, p.y, [self trackID]);
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    AppDelegate *del = [NSApp delegate];
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_up(GenericUI::Right, p.x, p.y, [self trackID]);
    if ([theEvent clickCount] == 2)
        del.ui->mouse_dblclick(GenericUI::Right, p.x, p.y, [self trackID]);
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    AppDelegate *del = [NSApp delegate];
    NSPoint p = [self windowCoordsToViewCoords:[theEvent locationInWindow]];
    del.ui->mouse_scroll([theEvent deltaY], p.x, [self trackID]);
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

@end
