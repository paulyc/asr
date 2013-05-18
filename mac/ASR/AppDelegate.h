//
//  AppDelegate.h
//  ASR
//
//  Created by Paul Ciarlo on 5/9/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#import <Cocoa/Cocoa.h>

class ASR;
class CocoaUI;

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property ASR *asr;
@property CocoaUI *ui;
@property (assign) IBOutlet NSWindow *window;

@end
