//
//  AppDelegate.h
//  ASR
//
//  Created by Paul Ciarlo on 5/9/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#import <Cocoa/Cocoa.h>

class ASR;

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    ASR *asr;
}

@property (assign) IBOutlet NSWindow *window;

@end
