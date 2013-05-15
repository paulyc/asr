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
    
	//	_detector.reset_source(_src);
    _src_buf = new BufferedStream<Chunk_T>(_src);
	//	_src_buf->load_complete();
    
    _filename = filename;
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
        BufferedSource<Chunk_T>::create(_io, filename.c_str(), lock);
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
