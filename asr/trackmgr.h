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

#ifndef _TRACKMGR_H
#define _TRACKMGR_H

#include <vector>

#include "track.h"

template <typename Chunk_T>
class TrackManager
{
public:
	typedef AudioTrack<Chunk_T> track_t;
	TrackManager() {}
	~TrackManager() {}

	track_t* createFromFile(const wchar_t *filename, pthread_mutex_t *lock)
	{
		track_t *t = new AudioTrack(_tracks.size(), filename, lock);
	}

	track_t *get(int indx)
	{
		return _tracks[indx];
	}

protected:
	std::vector<track_t*> _tracks;
};

#endif // !defined(TRACKMGR_H)
