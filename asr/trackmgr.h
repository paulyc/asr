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
