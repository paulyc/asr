//
//  MainViewController.h
//  mac
//
//  Created by Paul Ciarlo on 5/17/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface MainViewController : NSViewController

@property(nonatomic, setter = setStart:) BOOL start;

- (void)setStart:(BOOL)start;
- (void)button1Push;
- (void)button2Push;

@end
