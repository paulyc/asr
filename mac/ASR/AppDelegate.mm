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

- (void)renderTrack:(NSString*)trackid
{
    if ([trackid isEqualToString:@"1"])
        _ui->render_impl(1);
    else
        _ui->render_impl(2);
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
