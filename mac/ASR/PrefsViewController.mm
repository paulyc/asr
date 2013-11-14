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

NSDictionary *subviewIds = @{
							@"Drivers" : @1,
							@"Output1" : @2,
							@"Output2" : @3,
							@"Input1" : @4,
							};

- (id)getSubView:(NSString*)name
{
	NSNumber *num = subviewIds[name];
	return [[self view] viewWithTag:[num integerValue]];
}

- (void)setView:(NSView*)view
{
	[super setView:view];
	
	AppDelegate *del = [NSApp delegate];
	IOProcessor *io = del.ui->_io;
	_inputChannels = io->getInputChannels();
	_outputChannels = io->getOutputChannels();
	NSPopUpButton *drivers = [self getSubView:@"Drivers"];
	[drivers removeAllItems];
	[drivers addItemWithTitle:@"Core Audio"];
	
	NSPopUpButton *output1 = [self getSubView:@"Output1"];
	NSPopUpButton *output2 = [self getSubView:@"Output2"];
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
	
	NSPopUpButton *input1 = [self getSubView:@"Input1"];
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

- (void)configure
{
	AppDelegate *del = [NSApp delegate];
	IOProcessor *io = del.ui->_io;
	NSPopUpButton *output1 = [self getSubView:@"Output1"];
	NSInteger itemIndex = [output1 indexOfSelectedItem];
	io->setConfigOption(IOConfig::Output1Channel, _outputChannels[itemIndex].first);
	
	NSPopUpButton *output2 = [self getSubView:@"Output2"];
	itemIndex = [output2 indexOfSelectedItem];
	io->setConfigOption(IOConfig::Output2Channel, _outputChannels[itemIndex].first);
	
	NSPopUpButton *input1 = [self getSubView:@"Input1"];
	itemIndex = [input1 indexOfSelectedItem];
	io->setConfigOption(IOConfig::Input1Channel, _inputChannels[itemIndex].first);
	
	io->configure();
}

@end
