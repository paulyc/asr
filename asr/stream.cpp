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
    _streams[id] = stream(src);
    Worker::do_job(new Worker::generate_chunk_job<T_source<chunk_t>, chunk_t>(src, this, id, 0),
                   false, true);
    _lock.release();
}

chunk_t* ChunkGenerator::GetNextChunk(int streamID)
{
    chunk_t *chk = 0;
    _lock.acquire();
    chk = _streams[streamID]._nextChk;
    _streams[streamID]._nextChk = 0;
    if (!chk)
        chk = zero_source<chunk_t>::get()->next();
    else
    {
        Worker::do_job(new Worker::generate_chunk_job<T_source<chunk_t>, chunk_t>(_streams[streamID]._src, this, streamID, 0),
                       false, true);
    }
    _lock.release();
    return chk;
}

void ChunkGenerator::chunkCb(chunk_t *chunk, int id)
{
    _lock.acquire();
    _streams[id]._nextChk = chunk;
    _lock.release();
}
