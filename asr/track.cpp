//
//  track.cpp
//  mac
//
//  Created by Paul Ciarlo on 5/14/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

template <typename Chunk_T>
void BufferedSource<Chunk_T>::create(const char *filename, Lock_T *lock)
{
    T_source<Chunk_T> *src = 0;
    
    if (strstr(filename, ".mp3") == filename + strlen(filename) - 4)
    {
        src = new mp3file_chunker<Chunk_T>(filename, lock);
    }
    else if (strstr(filename, ".wav") == filename + strlen(filename) - 4)
    {
        src = new wavfile_chunker<Chunk_T>(filename);
        //src = new ifffile_chunker<Chunk_T>(filename);
    }
    else if (strstr(filename, ".flac") == filename + strlen(filename) - 5)
    {
        src = new flacfile_chunker<Chunk_T>(filename, lock);
    }
    else
    {
        src = new ifffile_chunker<Chunk_T>(filename);
    }
    
    destroy();
    
    _src = src;
    _src_buf = new BufferedStream<Chunk_T>(_src);
	//	_src_buf->load_complete();
    
    _filename = filename;
}

template <typename Chunk_T>
AudioTrack<Chunk_T>::AudioTrack(ASIOProcessor *io, int track_id, const char *filename) :
_io(io),
_in_config(false),
_paused(true),
_loaded(true),
_track_id(track_id),
_last_time(0.0),
_pause_monitor(false)
{
    _future = new FutureExecutor;
    
    if (filename)
        set_source_file_impl(std::string(filename), &_io->get_lock());
}

template <typename Chunk_T>
AudioTrack<Chunk_T>::~AudioTrack()
{
    delete _future;
    _future = 0;
    
    _loading_lock.acquire();
    while (!_loaded)
        _track_loaded.wait(_loading_lock);
    _loading_lock.release();
    
    ViewableMixin<Chunk_T>::destroy();
    PitchableMixin<Chunk_T>::destroy();
    
    BufferedSource<Chunk_T>::destroy();
}

template <typename Chunk_T>
void AudioTrack<Chunk_T>::set_source_file(std::string filename, Lock_T &lock)
{
    _future->submit(
                    new deferred2<AudioTrack<Chunk_T>, std::string, Lock_T*>(
                                                                             this,
                                                                             &AudioTrack<Chunk_T>::set_source_file_impl, 
                                                                             filename, 
                                                                             &lock));
}

template <typename Chunk_T>
void AudioTrack<Chunk_T>::set_source_file_impl(std::string filename, Lock_T *lock)
{
    _loading_lock.acquire();
    while (_in_config || !_loaded)
    {
        _track_loaded.wait(_loading_lock);
    }
    
    _in_config = true;
    _loaded = false;
    _paused = true;
    
    try {
        BufferedSource<Chunk_T>::create(filename.c_str(), lock);
    } catch (std::exception &e) {
        printf("Could not set_source_file due to %s\n", e.what());
        _in_config = false;
        _loaded = true;
        _paused = true;
		//	BufferedSource<Chunk_T>::createZero(_io);
        _loading_lock.release();
        return;
    }
	//	pthread_mutex_unlock(&_loading_lock);
    
	//	LOCK_IF_SMP(lock);
    PitchableMixin<Chunk_T>::create(this->_src_buf, this->_src->sample_rate());
    ViewableMixin<Chunk_T>::create(this->_src_buf);
    this->_cuepoint = 0.0;
	//	UNLOCK_IF_SMP(lock);
    
	//	pthread_mutex_lock(&_loading_lock);
    _in_config = false;
    _last_time = 0.0;
    _filename = filename;
    _loading_lock.release();
    
    if (!_loaded)
        Worker::do_job(new Worker::load_track_job<AudioTrack<Chunk_T> >(this, lock));
}

template <typename Chunk_T>
Chunk_T* AudioTrack<Chunk_T>::next()
{
    Chunk_T *chk;
    
    _loading_lock.acquire();
    if (!_loaded || (_paused && !_pause_monitor))
    {
        _loading_lock.release();
        chk = zero_source<Chunk_T>::get()->next();
    }
    else
    {
        if (_paused && _pause_monitor)
        {
            double t = this->_resample_filter->get_time();
            chk = this->_resample_filter->next();
            this->_resample_filter->seek_time(t);
        }
        else
        {
            chk = this->_resample_filter->next();
            if (true && this->_resample_filter->get_time() >= len().time)
                this->_resample_filter->seek_time(0.0);
        }
        _loading_lock.release();
        render();
    }
    
    return chk;
}

template <typename Chunk_T>
bool AudioTrack<Chunk_T>::load_step(Lock_T *lock)
{
    this->_meta->load_metadata(lock, _io);
    
    printf("len %d samples %d chunks\n", this->_src_buf->len().samples, this->_src_buf->len().chunks);
    
    this->_display->set_zoom(1.0);
    this->_display->set_left(0.0);
    //pthread_mutex_lock(lock);
    this->_display->set_wav_heights(lock);
	//	if (!_display->set_next_height())
	//	{
    //	GenericUI *ui;
	//		pthread_mutex_unlock(lock);
    
    this->_detector.reset_source(this->_src_buf, lock);
    
    this->_resample_filter->set_output_scale(1.0f / this->_src->maxval());
    //_meta->load_metadata(lock);
    _loading_lock.acquire();
    _loaded = true;
    
    if (_io && _io->get_ui())
    {
        _io->get_ui()->render(_track_id);
    }
    
    _track_loaded.signal();
    _loading_lock.release();
    
    _io->get_ui()->get_track(_track_id).filename.set_text(_filename.c_str(), false);
    
    return false;
	//	}
    //pthread_mutex_unlock(lock);
}
