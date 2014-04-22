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

#include "AudioDevice.h"

#include <math.h>
#if IOS
#include <CoreAudio/CoreAudioTypes.h>
#else
#include <CoreAudio/CoreAudio.h>
#endif

SineAudioOutputProcessor::SineAudioOutputProcessor(float32_t frequency, const IAudioStreamDescriptor *streamDesc) :
	_frequency(frequency),
	_time(0.0)
{
	_sampleRate = streamDesc->GetSampleRate();
	_frameSize = streamDesc->GetNumChannels() * streamDesc->GetSampleWordSizeInBytes();
}
	
void SineAudioOutputProcessor::ProcessOutput(IAudioBuffer *buffer)
{
	float32_t *sample = (float32_t*)buffer->GetBuffer();
	int bufferSizeFrames = buffer->GetBufferSize() / _frameSize;
	for (int i = 0; i < bufferSizeFrames; ++i)
	{
		const float32_t smp = 0.7*sin(2*M_PI*_frequency*_time);
		sample[i*8+2] = smp;
		sample[i*8+3] = smp;
		_time += 1.0 / _sampleRate;
	}
}

#if IOS

IAudioStream *iOSAudioOutputStreamDescriptor::GetStream() const
{
	return new iOSAudioOutputStream(*this);
}

#endif // IOS

CoreAudioInput::CoreAudioInput(int id, int ch1ofs, int ch2ofs) :
_id(id),
_ch1ofs(ch1ofs),
_ch2ofs(ch2ofs),
_nextChunk(0)
{
	_nextChunk = T_allocator<chunk_t>::alloc();
	_writePtr = _nextChunk->_data;
}

CoreAudioInput::~CoreAudioInput()
{
	T_allocator<chunk_t>::free(_nextChunk);
	_lock.acquire();
	while (!_chunkQ.empty())
	{
		T_allocator<chunk_t>::free(_chunkQ.front());
		_chunkQ.pop();
	}
	_lock.release();
}

void CoreAudioInput::process(MultichannelAudioBuffer *buf)
{
	if (_ch1ofs >= buf->GetNumChannels())
		return;
	const int stride = buf->GetStride();
	int to_read = buf->GetBufferSizeFrames(), loop_read, bytes_read=0, chunk_left;
	float32_t * const read = (float32_t* const)buf->GetBuffer();
	
	while (to_read > 0)
	{
		if (_writePtr - _nextChunk->_data >= chunk_t::chunk_size)
		{
			_lock.acquire();
			_chunkQ.push(_nextChunk);
			_chunkAvailable.signal();
			_lock.release();
			_nextChunk = T_allocator<chunk_t>::alloc();
			_writePtr = _nextChunk->_data;
		}
		chunk_left = chunk_t::chunk_size - (_writePtr - _nextChunk->_data);
		loop_read = std::min(to_read, chunk_left);
		for (int i = 0; i < loop_read; ++i)
		{
			_writePtr[i][0] = read[i*stride+_ch1ofs];
			_writePtr[i][1] = read[i*stride+_ch2ofs];
		}
		
		_writePtr += loop_read;
		to_read -= loop_read;
		bytes_read += loop_read;
	}
}

chunk_t *CoreAudioInput::next()
{
	chunk_t *chk = 0;
	_lock.acquire();
	while (_chunkQ.empty())
	{
		_chunkAvailable.wait(_lock);
	}
	chk = _chunkQ.front();
	_chunkQ.pop();
	_lock.release();
	return chk;
}

void CoreAudioInput::stop()
{
	_lock.acquire();
	_chunkQ.push(0);
	_chunkAvailable.signal();
	_lock.release();
}

CoreAudioInputProcessor::CoreAudioInputProcessor(const IAudioStreamDescriptor *streamDesc)
{
	_channels = streamDesc->GetNumChannels();
	_frameSize = _channels * streamDesc->GetSampleWordSizeInBytes();
}

void CoreAudioInputProcessor::ProcessInput(IAudioBuffer *buffer)
{
	for (auto in: _inputs)
	{
		in->process(dynamic_cast<MultichannelAudioBuffer*>(buffer));
	}
}

void CoreAudioOutput::process(MultichannelAudioBuffer *buf)
{
	if (_ch1id >= buf->GetNumChannels())
		return;
	const int stride = buf->GetStride();
	int to_write = buf->GetBufferSizeFrames(), loop_write, written=0, chunk_left;
	float32_t * const write = (float32_t* const)buf->GetBuffer();
	
	while (to_write > 0)
	{
		if (!_chk || _read - _chk->_data >= chunk_t::chunk_size)
		{
			T_allocator<chunk_t>::free(_chk);
			_chk = _gen->GetNextChunk(_id);
			_read = _chk->_data;
		}
		chunk_left = chunk_t::chunk_size - (_read - _chk->_data);
		loop_write = std::min(to_write, chunk_left);
		for (int i = 0; i < loop_write; ++i)
		{
			write[i*stride+_ch1id] = _read[i][0];
			write[i*stride+_ch2id] = _read[i][1];
			
			if (!_clip && (_read[i][0] > 1.0f || _read[i][1] > 1.0f))
			{
				_clip = true;
				printf("clip\n");
			}
		}
		
		_read += loop_write;
		to_write -= loop_write;
		written += loop_write;
	}
}

CoreAudioOutputProcessor::CoreAudioOutputProcessor(/*const IAudioStreamDescriptor *streamDesc*/)
{
	//_channels = streamDesc->GetNumChannels();
	//_frameSize = _channels * streamDesc->GetSampleWordSizeInBytes();
}

CoreAudioOutputProcessor::~CoreAudioOutputProcessor()
{
	for (auto out: _outputs)
	{
		delete out;
	}
}

void CoreAudioOutputProcessor::ProcessOutput(IAudioStream *stream, IAudioBuffer *buffer)
{
	for (auto out: _outputs)
	{
		if (out->getStream() == stream) out->process(dynamic_cast<MultichannelAudioBuffer*>(buffer));
	}
}
