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

#import <Cocoa/Cocoa.h>
#import "../../asr/track.h"

@interface MainViewController : NSViewController

@property(nonatomic, setter = setStart:) BOOL start;

@property(getter = getPitch1, setter = setPitch1:) float pitch1;
@property(getter = getPitch2, setter = setPitch2:) float pitch2;

- (void)setStart:(BOOL)start;
- (void)button1Push;
- (void)button2Push;
- (BOOL)track1Active;
- (void)playTrack1;
- (void)playTrack2;
- (void)setCuepoint1;
- (void)setCuepoint2;
- (void)gotoCuepoint1;
- (void)gotoCuepoint2;
- (void)showPrefs;
- (typename IOProcessor::track_t*)getTrack:(int)track_id;

@end
