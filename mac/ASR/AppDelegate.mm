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

@implementation AppDelegate

- (void)dealloc
{
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insert code here to initialize your application
    
    BeatDetector<chunk_t>::test_main();
    /*
    NSOpenPanel* openDlg = [NSOpenPanel openPanel];
    
    [openDlg setCanChooseFiles:YES];
    [openDlg setCanChooseDirectories:NO];
    [openDlg setAllowsMultipleSelection:NO];
    
    if ([openDlg runModal] == NSOKButton)
    {
        NSArray* files = [openDlg URLs];
        
        for( int i = 0; i < [files count]; i++ )
        {
            NSString* nsFileName = [files objectAtIndex:i];
            NSLog(@"%@", nsFileName);
        }
    }*/
}

@end
