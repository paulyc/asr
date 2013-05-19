//
//  MyOpenGLView.m
//  mac
//
//  Created by Paul Ciarlo on 5/17/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#import "MyOpenGLView.h"

@implementation MyOpenGLView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code here.
    }
    
    return self;
}

- (void)awakeFromNib
{
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
