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
    _streams[id] = new Worker::generate_chunk_loop<T_source<chunk_t>, chunk_t>();
    Worker::do_job(_streams[id], false, true);
    _lock.release();
}

chunk_t* ChunkGenerator::GetNextChunk(int streamID)
{
    chunk_t *chk = 0;
    _lock.acquire();
    if (_streams[streamID]._nextChks.empty())
        chk = zero_source<chunk_t>::get()->next();
    else
    {
        chk = _streams[streamID]._nextChks.front();
        _streams[streamID]._nextChks.pop();
        GenerateChunk(streamID);
    }
    _lock.release();
    return chk;
}

void ChunkGenerator::GenerateChunk(int streamID)
{
    Worker::do_job(new Worker::generate_chunk_job<T_source<chunk_t>, chunk_t>(_streams[streamID]._src, this, streamID, 0),
                   false, true);
}

void ChunkGenerator::chunkCb(chunk_t *chunk, int id)
{
    _lock.acquire();
    _streams[id]._nextChks.push(chunk);
    if (_streams[id]._nextChks.size() < _chunksToBuffer)
    {
        GenerateChunk(id);
    }
    _lock.release();
}
