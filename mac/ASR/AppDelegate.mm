//
//  AppDelegate.mm
//  ASR
//
//  Created by Paul Ciarlo on 5/9/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#import "AppDelegate.h"

#include "asr.h"
#include "ui.h"
#include "beats.h"
#include "ui_cocoa.hpp"

@implementation AppDelegate

- (void)dealloc
{
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
   // NSView *view = [_window contentView];
 //   NSNotificationCenter *ncenter = [NSNotificationCenter defaultCenter];
 //   [ncenter addObserver:self selector:@selector(evtCallback:) name:nil object:_window];
    _ui = new CocoaUI;
    _asr = new ASR(_ui);
    _asr->init();
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    _asr->finish();
    delete _asr;
#if DEBUG_MALLOC
	dump_malloc();
#endif
    return NSTerminateNow;
}

@end
