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
#import "MainViewController.h"
#import "PrefsViewController.h"
#import "../../asr/ui.h"

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

- (void)playTrack2
{
    AppDelegate *del = [NSApp delegate];
    del.ui->play_pause(2);
}

- (BOOL)track1Active
{
    return YES;
}

- (void)showPrefs
{
	//AppDelegate *del = [NSApp delegate];
	[[PrefsViewController alloc] initWithNibName:@"PrefsWindow" bundle:nil];
}

@end
