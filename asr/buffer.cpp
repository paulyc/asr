//
//  buffer.cpp
//  mac
//
//  Created by Paul Ciarlo on 5/14/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

template <typename Chunk_T>
BufferedStream<Chunk_T>::BufferedStream(T_source<Chunk_T> *src, double smp_rate, bool preload) :
FilterSource<Chunk_T>(src),
_chk_ofs(0),
_smp_ofs(0),
_chk_ofs_loading(0),
_buffer_p(0),
_sample_rate(smp_rate)
{
    int chks = len().chunks;
    if (chks != -1)
        _chks.resize(chks, 0);
    
    _sample_period = 1.0 / _sample_rate;
    
    _chks.resize(10000, 0);
}

template <typename Chunk_T>
BufferedStream<Chunk_T>::~BufferedStream()
{
    typename std::vector<Chunk_T*>::iterator i;
    for (i = _chks.begin(); i != _chks.end(); ++i)
    {
        T_allocator<Chunk_T>::free(*i);
    }
}

template <typename Chunk_T>
Chunk_T *BufferedStream<Chunk_T>::get_chunk(unsigned int chk_ofs)
{
    // dont think this needs locked??
    if (this->_src->len().chunks != -1 && chk_ofs >= uchk_ofs_t(this->_src->len().chunks))
    {
        return zero_source<Chunk_T>::get()->next();
    }
    _buffer_lock.acquire();
    if (chk_ofs >= _chks.size())
        _chks.resize(chk_ofs*2, 0);
    if (!_chks[chk_ofs])
    {
		//	printf("Shouldnt happen anymore\n");
        _buffer_lock.release();
        // possible race condition where mp3 chunk not loaded
        try
        {
            _src_lock.acquire();
            this->_src->seek_chk(chk_ofs);
        }
        catch (std::exception e)
        {
            printf("%s\n", e.what());
            _src_lock.release();
            return zero_source<Chunk_T>::get()->next();
        }
        Chunk_T *c = this->_src->next();
        _src_lock.release();
        _buffer_lock.acquire();
        _chks[chk_ofs] = c;
    }
    Chunk_T *r = _chks[chk_ofs];
    _buffer_lock.release();
    return r;
}

template <typename Chunk_T>
Chunk_T *BufferedStream<Chunk_T>::next()
{
    return get_chunk(_chk_ofs++);
}

template <typename Chunk_T>
void BufferedStream<Chunk_T>::seek_smp(smp_ofs_t smp_ofs)
{
    _data_lock.acquire();
    _smp_ofs = smp_ofs;
    if (_smp_ofs < 0)
        _chk_ofs = 0;
    else
        _chk_ofs = smp_ofs / Chunk_T::chunk_size;
    _data_lock.release();
}

template <typename Chunk_T>
int BufferedStream<Chunk_T>::get_samples(double tm, typename Chunk_T::sample_t *buf, int num)
{
    smp_ofs_t ofs = tm * this->_src->sample_rate();
    return get_samples(ofs, buf, num);
}

template <typename Chunk_T>
int BufferedStream<Chunk_T>::get_samples(smp_ofs_t ofs, typename Chunk_T::sample_t *buf, int num)
{
    while (ofs < 0 && num > 0)
    {
        (*buf)[0] = 0.0;
        (*buf)[1] = 0.0;
        ++ofs;
        --num;
        ++buf;
    }
    usmp_ofs_t chk_ofs = (usmp_ofs_t)ofs / Chunk_T::chunk_size,
    smp_ofs = (usmp_ofs_t)ofs % Chunk_T::chunk_size;
    int chk_left, to_cpy, written=0;
    do
    {
        chk_left = Chunk_T::chunk_size - smp_ofs;
        to_cpy = std::min(chk_left, num);
        _buffer_lock.acquire();
        if (chk_ofs >= _chks.size())
            _chks.resize(chk_ofs*2, 0);
        if (!_chks[chk_ofs])
        {
            _buffer_lock.release();
            _src_lock.acquire();
            Chunk_T *c = 0;
            if (this->_src->len().chunks != -1 && usmp_ofs_t(this->_src->len().chunks) <= chk_ofs)
                c = zero_source<Chunk_T>::get()->next();
            else
            {
                this->_src->seek_chk(chk_ofs);
                c = this->_src->next();
            }
            _src_lock.release();
            _buffer_lock.acquire();
            _chks[chk_ofs] = c;
        }
        
        // copy bits
        memcpy(buf, _chks[chk_ofs]->_data + smp_ofs, sizeof(typename Chunk_T::sample_t)*to_cpy);
        _buffer_lock.release();
        
        if (to_cpy == chk_left)
        {
            ++chk_ofs;
            smp_ofs = 0;
        }
        buf += to_cpy;
        num -= to_cpy;
        written += to_cpy;
    } while (num > 0);
    
    return written;
}

template <typename Chunk_T>
typename T_source<Chunk_T>::pos_info& BufferedStream<Chunk_T>::len()
{
    return this->_src->len();
}

template <typename Chunk_T>
bool BufferedStream<Chunk_T>::load_next()
{
    if (this->_src->eof())
    {
        this->_len.chunks = _chk_ofs_loading-1;
        _src_lock.acquire();
        this->_src->len().chunks = _chk_ofs_loading-1;
        this->_src->len().samples = this->_src->len().chunks*Chunk_T::chunk_size;
        this->_src->len().smp_ofs_in_chk = this->_src->len().samples % this->_src->len().chunks;
        this->_src->len().time = this->_src->len().samples/this->_src->sample_rate();
        _src_lock.release();
        return false;
    }
    next_chunk(_chk_ofs_loading++);
    return true;
}

template <typename Chunk_T>
typename BufferedStream<Chunk_T>::Sample_T* BufferedStream<Chunk_T>::load(double tm)
{
    this->_src->get_samples(tm, _buffer, BufSz);
    _buffer_tm = tm;
    _buffer_p = _buffer;
    return _buffer_p;
}

template <typename Chunk_T>
typename BufferedStream<Chunk_T>::Sample_T* BufferedStream<Chunk_T>::get_at(double tm, int num_samples)
{
    if (_buffer_p)
    {
        double tm_diff = tm-_buffer_tm;
        if (tm_diff < 0.0)
        {
            return load(tm);
        }
        else
        {
            smp_ofs_t ofs = tm_diff * this->_src->sample_rate();
            if (ofs+num_samples >= BufSz)
            {
                return load(tm);
            }
            else
            {
                return _buffer_p + ofs;
            }
        }
    }
    else
    {
        return load(tm);
    }
}

template <typename Chunk_T>
typename BufferedStream<Chunk_T>::Sample_T* BufferedStream<Chunk_T>::get_at_ofs(smp_ofs_t ofs, int n)
{
    get_samples(ofs, _buffer, n);
    return _buffer;
}
