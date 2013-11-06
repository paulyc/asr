// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo
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
#import "PrefsViewController.h"
#import "../../asr/io.h"

@interface PrefsViewController ()

@end

@implementation PrefsViewController

- (void)setView:(NSView*)view
{
	[super setView:view];
	
	AppDelegate *del = [NSApp delegate];
	IOProcessor *io = del.ui->_io;
	_inputChannels = io->getInputChannels();
	_outputChannels = io->getOutputChannels();
	NSPopUpButton *drivers = [view viewWithTag:1];
	[drivers removeAllItems];
	[drivers addItemWithTitle:@"Core Audio"];
	
	NSPopUpButton *output1 = [view viewWithTag:2];
	NSPopUpButton *output2 = [view viewWithTag:3];
	[output1 removeAllItems];
	[output2 removeAllItems];
	
	const int output1ch = io->getConfigOption(IOConfig::Output1Channel);
	const int output2ch = io->getConfigOption(IOConfig::Output2Channel);
	int index = 0;
	for (const auto &pair : _outputChannels)
	{
		[output1 addItemWithTitle:[[NSString alloc] initWithUTF8String:pair.second.c_str()]];
		[output2 addItemWithTitle:[[NSString alloc] initWithUTF8String:pair.second.c_str()]];
		
		if (pair.first == output1ch)
			[output1 selectItemAtIndex:index];
		if (pair.first == output2ch)
			[output2 selectItemAtIndex:index];
		
		++index;
	}
	
	NSPopUpButton *input1 = [view viewWithTag:4];
	[input1 removeAllItems];
	
	const int input1ch = io->getConfigOption(IOConfig::Output2Channel);
	index = 0;
	for (const auto &pair : _inputChannels)
	{
		[input1 addItemWithTitle:[[NSString alloc] initWithUTF8String:pair.second.c_str()]];
		
		if (pair.first == input1ch)
			[input1 selectItemAtIndex:index];
		
		++index;
	}
}

@end
