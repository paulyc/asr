//
//  stream.cpp
//  mac
//
//  Created by Paul Ciarlo on 5/14/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#include "stream.h"
#include "worker.h"

void ChunkGenerator::AddChunkSource(T_source<chunk_t> *src, int id)
{
    _lock.acquire();
    Worker::generate_chunk_loop<T_source<chunk_t>, chunk_t> *job = new Worker::generate_chunk_loop<T_source<chunk_t>, chunk_t>(src, this, id, _chunksToBuffer);
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
  //  _lock.acquire();
  //  if (_lockMask == 0)
        _ioLock->acquire();
  //  _lockMask |= 1 << id;
  //  _lock.release();
}

void ChunkGenerator::unlock(int id)
{
 //   _lock.acquire();
 //   _lockMask &= 1 << id;
 //   if (_lockMask == 0)
        _ioLock->release();
 //   _lock.release();
}
