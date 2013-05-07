#if WINDOWS

/*
template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T, int chunk_size>
void asio_sink<Input_Sample_T, Output_Sample_T, Chunk_T, chunk_size, false>::process()
{
	// assume 2 channels non-interleaved same data format
	memcpy(asio->_buffer_infos[2].buffers[asio->_doubleBufferIndex], t->_data, sizeof(t->_data)/2);
	memcpy(asio->_buffer_infos[3].buffers[asio->_doubleBufferIndex], (char*)(t->_data)+sizeof(t->_data)/2, sizeof(t->_data)/2);
}
*/

template <typename Chunk_T, typename Source_T, typename Output_Sample_T>
void asio_sink<Chunk_T, Source_T, Output_Sample_T>::switchBuffers(int dbIndex)
{
	memcpy(_buffers[0][dbIndex], _bufL, _buf_size*sizeof(Output_Sample_T));
	memcpy(_buffers[1][dbIndex], _bufR, _buf_size*sizeof(Output_Sample_T));
}

template <typename Chunk_T, typename Source_T, typename Output_Sample_T>
void asio_sink<Chunk_T, Source_T, Output_Sample_T>::process()
{
	int to_write = _buf_size, loop_write, written=0;
	int stride = 2;
	Output_Sample_T *write, *end_write;
	typename Chunk_T::sample_t *read;
	while (to_write > 0)
	{
		if (!_chk || _read - _chk->_data >= chunk_t::chunk_size)
		{
			T_allocator<chunk_t>::free(_chk);
			_chk = 0;

			pthread_mutex_lock(&this->_src_lock);
			_chk = _src_t->next();
			pthread_mutex_unlock(&this->_src_lock);
			_read = _chk->_data;
		}
		loop_write = min(to_write, chunk_t::chunk_size - (_read - _chk->_data));
		for (write = _bufL + written,
			end_write = write + loop_write,
			read = _read;
			write != end_write;
			++write, ++read)
		{
		//	*write = Output_Sample_T((*read)[0] * MaxVal<Output_Sample_T>::val);
			if ((*read)[0] < 0.0f)
				*write = Output_Sample_T(-max(-1.0f, (*read)[0]) * MinVal<Output_Sample_T>::val);
			else
				*write = Output_Sample_T(min(1.0f, (*read)[0]) * MaxVal<Output_Sample_T>::val);
		}
		for (write = _bufR + written,
			end_write = write + loop_write,
			read = _read;
			write != end_write;
			++write, ++read)
		{
		//	*write = Output_Sample_T((*read)[1] * MaxVal<Output_Sample_T>::val);
			if ((*read)[1] < 0.0f)
				*write = Output_Sample_T(-max(-1.0f, (*read)[1]) * MinVal<Output_Sample_T>::val);
			else
				*write = Output_Sample_T(min(1.0f, (*read)[1]) * MaxVal<Output_Sample_T>::val);
		}
		_read += loop_write;
		to_write -= loop_write;
		written += loop_write;
	}
}

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
asio_source<Input_Sample_T, Output_Sample_T, Chunk_T>::asio_source(T_sink<chunk_t> *sink, size_t buf_sz, Input_Sample_T **bufsL, Input_Sample_T **bufsR) :
	_sink(sink),
	_buf_sz(buf_sz),
	_bufsL(bufsL),
	_bufsR(bufsR)
{
	_chk_working = T_allocator<Chunk_T>::alloc();
	_chk_ptr = _chk_working->_data;
	sem_init(&_next_sem, 0, 1);
}

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
asio_source<Input_Sample_T, Output_Sample_T, Chunk_T>::~asio_source()
{
	sem_destroy(&_next_sem);
	T_allocator<Chunk_T>::free(_chk_working);
	delete _sink;
}

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
void asio_source<Input_Sample_T, Output_Sample_T, Chunk_T>::copy_data(long doubleBufferIndex)
{
	Input_Sample_T *input0 = _bufsL[doubleBufferIndex], 
		*input1 = _bufsR[doubleBufferIndex], 
		*end0 = input0 + _buf_sz, 
		*end1 = input1 + _buf_sz;
	SamplePairf *end_chk = _chk_working->_data + chunk_t::chunk_size;
	do
	{
		while (input0 < end0 && input1 < end1 && _chk_ptr < end_chk)
		{
		//	(*_chk_ptr)[0] = *input0++ / float(MaxVal<Input_Sample_T>::val);
			if (*input0 >= 0)
				(*_chk_ptr)[0] = *input0++ / float(MaxVal<Input_Sample_T>::val);
			else
				(*_chk_ptr)[0] = -*input0++ / float(MinVal<Input_Sample_T>::val);

		//	(*_chk_ptr)[1] = *input1++ / float(MaxVal<Input_Sample_T>::val);
			if (*input1 >= 0)
				(*_chk_ptr++)[1] = *input1++ / float(MaxVal<Input_Sample_T>::val);
			else
				(*_chk_ptr++)[1] = -*input1++ / float(MinVal<Input_Sample_T>::val);
		}
		if (_chk_ptr == end_chk)
		{
			sem_wait(&_next_sem);
			_chks_next.push(_chk_working);
			sem_post(&_next_sem);
			_chk_working = T_allocator<chunk_t>::alloc();
			_chk_ptr = _chk_working->_data;
			end_chk = _chk_ptr + chunk_t::chunk_size;
		}
		if (input0 == end0)
			break;
	} while (true);
}

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
Chunk_T* asio_source<Input_Sample_T, Output_Sample_T, Chunk_T>::next()
{
	while (1)
	{
		sem_wait(&_next_sem);
		if (_chks_next.size() > 0)
			break;
		sem_post(&_next_sem);
	}
	Chunk_T *n = _chks_next.front();
	_chks_next.pop();
	sem_post(&_next_sem);
	return n;
}

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
void asio_source<Input_Sample_T, Output_Sample_T, Chunk_T>::process(int doubleBufferIndex)
{
	copy_data(doubleBufferIndex);
	while (_sink && chunk_ready())
	{
		_sink->process();
	}
}

ASIOProcessor *MyASIOCallbacks::io = 0;

/*
Purpose:
bufferSwitchTimeInfo indicates that both input and output are to be processed. It also passes
extended time information (time code for synchronization purposes) to the host application and
back to the driver.
*/
void MyASIOCallbacks::bufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
{
	io->BufferSwitch(doubleBufferIndex, directProcess);
}

ASIOTime* MyASIOCallbacks::bufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess)
{
	bufferSwitch(doubleBufferIndex, directProcess);
	return params;
}

/*
This callback will inform the host application that a sample rate change was detected (e.g. sample
rate status bit in an incoming S/PDIF or AES/EBU signal changes).
*/
void MyASIOCallbacks::sampleRateDidChange(ASIOSampleRate sRate)
{// dont care
}

long MyASIOCallbacks::asioMessage(long selector, long value, void *message, double *opt)
{
	switch (selector)
	{
	case kAsioSelectorSupported:
		switch (value)
		{
		case kAsioSelectorSupported:
		case kAsioSupportsTimeInfo:
		case kAsioEngineVersion:
			return 1L;
		default:
			return 0;
		}
		break;
	case kAsioSupportsTimeInfo:
//		return 1L;
		return 0;
	case kAsioEngineVersion:
		return 2;
	default:
		return 0;
	}
	return ASE_OK;
}

#endif // WINDOWS
