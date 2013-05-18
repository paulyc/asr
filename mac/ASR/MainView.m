//
//  MainView.m
//  mac
//
//  Created by Paul Ciarlo on 5/17/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#import "MainView.h"

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
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect
{
    // Drawing code here.
    [super drawRect:dirtyRect];
}

- (void)mouseUp:(NSEvent *)ev
{
    
}

@end
