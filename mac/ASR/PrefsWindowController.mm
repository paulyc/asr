//
//  PrefsWindowController.m
//  mac
//
//  Created by Paul Ciarlo on 11/5/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#import "PrefsWindowController.h"

@interface PrefsWindowController ()

@end

@implementation PrefsWindowController

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        
    }
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.

	_viewController = [PrefsViewController alloc];
	[_viewController setView:[[self window] contentView]];
}

- (void)configure
{
	[_viewController configure];
	[super close];
}

@end
