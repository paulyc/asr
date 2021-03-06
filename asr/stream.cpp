// ASR - Digital Signal Processor
// Copyright (C) 2002-2013	Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

#include "stream.h"
#include "worker.h"

#include "AudioTypes.h"

ChunkGenerator::ChunkGenerator(int bufferSizeFrames, CriticalSectionGuard *ioLock) : _ioLock(ioLock), _lockMask(0)
{
	_chunksToBuffer = bufferSizeFrames / chunk_t::chunk_size;
	if (bufferSizeFrames % chunk_t::chunk_size > 0)
		++_chunksToBuffer;
	printf("chunksToBuffer = %d\n", _chunksToBuffer);
}

void ChunkGenerator::AddChunkSource(T_source<chunk_t> *src, int id)
{
	_lock.acquire();
	generate_chunk_loop *job = new generate_chunk_loop(src, this, id, _chunksToBuffer);
	_streams[id] = job;
	Worker::do_job(job, false, true);
	_lock.release();
}

chunk_t* ChunkGenerator::GetNextChunk(int streamID)
{
	chunk_t *chk = 0;
	_lock.acquire();
	chk = _streams[streamID]->get();
	_lock.release();
	return chk;
}

void ChunkGenerator::lock(int id)
{
   // _ioLock->enter();
//	T_allocator<chunk_t>::lock();
//	Worker::suspend_all();
//	T_allocator<chunk_t>::unlock();
}

void ChunkGenerator::unlock(int id)
{
   // _ioLock->leave();
//	T_allocator<chunk_t>::lock();
//	Worker::unsuspend_all();
//	T_allocator<chunk_t>::unlock();
}

void ChunkGenerator::kill()
{
	_lock.acquire();
	for (std::unordered_map<int, IChunkGeneratorLoop<chunk_t>*>::iterator i = _streams.begin();
		 i != _streams.end();
		 i++)
	{
		i->second->kill();
	}
	_lock.release();
}
