//
//  MainViewController.m
//  mac
//
//  Created by Paul Ciarlo on 5/17/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#import "AppDelegate.h"
#import "MainViewController.h"
#import "../asr/ui.h"

@interface MainViewController ()

@end

@implementation MainViewController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Initialization code here.
    }
    
    return self;
}

- (void)setStart:(BOOL)status
{
    AppDelegate *del = [NSApp delegate];
    if (status == YES)
        del.ui->_io->Start();
    else
        del.ui->_io->Stop();
}

- (void)button1Push
{
    AppDelegate *del = [NSApp delegate];
    std::string filename;
    if (FileOpenDialog::OpenSingleFile(filename))
    {
        del.ui->drop_file(filename.c_str(), true);
    }
}

- (void)button2Push
{
    AppDelegate *del = [NSApp delegate];
    std::string filename;
    if (FileOpenDialog::OpenSingleFile(filename))
    {
        del.ui->drop_file(filename.c_str(), false);
    }
}

- (void)playTrack1
{
    AppDelegate *del = [NSApp delegate];
    del.ui->play_pause(1);
}

@end
