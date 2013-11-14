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
#import "PrefsWindowController.h"
#import "../../asr/ui.h"

@interface MainViewController ()

@end

@implementation MainViewController

const float pitchRange = 0.08;
float pitch1Value = 500.0f;
float pitch2Value = 500.0f;

- (typename IOProcessor::track_t*)getTrack:(int)track_id
{
	AppDelegate *del = [NSApp delegate];
	return del.ui->_io->GetTrack(track_id);
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

- (void)setCuepoint1
{
	[self getTrack:1]->set_cuepoint([self getTrack:1]->get_playback_time());
}

- (void)setCuepoint2
{
	[self getTrack:2]->set_cuepoint([self getTrack:2]->get_playback_time());
}

- (void)gotoCuepoint1
{
	[self getTrack:1]->goto_cuepoint(false);
}

- (void)gotoCuepoint2
{
	[self getTrack:2]->goto_cuepoint(false);
}

- (void)setPitch1:(float)value
{
	pitch1Value = value;
	const double pitchValue = (1.0 - pitchRange) + 2.0*pitchRange * value/1000.0f;
	[self getTrack:1]->set_pitch(pitchValue);
}

- (void)setPitch2:(float)value
{
	pitch2Value = value;
	const double pitchValue = (1.0 - pitchRange) + 2.0*pitchRange * value/1000.0f;
	[self getTrack:2]->set_pitch(pitchValue);
}

- (float)getPitch1
{
	return pitch1Value;
}

- (float)getPitch2
{
	return pitch2Value;
}

- (BOOL)track1Active
{
    return YES;
}

- (void)showPrefs
{
	//NSWindowController * wc = [[[NSWindowController alloc] initWithNibName:@"PrefsWindow" bundle:nil]];
	[[[PrefsWindowController alloc] initWithWindowNibName:@"PrefsWindow"] window];
}

@end
