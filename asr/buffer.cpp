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
/*
template <typename Chunk_T>
Chunk_T *BufferedStream<Chunk_T>::next_chunk(smp_ofs_t chk_ofs)
{
    Chunk_T *c;
    _buffer_lock.acquire();
    if (chk_ofs < 0)
    {
        return zero_source<Chunk_T>::get()->next();
    }
    uint32_t uchk_ofs = (uint32_t)chk_ofs;
    if (uchk_ofs >= _chks.size())
        _chks.resize(uchk_ofs*2, 0);
    if (!_chks[uchk_ofs])
    {
        _buffer_lock.release();
        _src_lock.acquire();
        c = this->_src->next();
        _src_lock.release();
        _buffer_lock.acquire();
        _chks[uchk_ofs] = c;
    }
    c = _chks[uchk_ofs];
    _buffer_lock.release();
    return c;
}*/

template <typename Chunk_T>
Chunk_T *BufferedStream<Chunk_T>::next()
{
    return get_chunk(_chk_ofs++);
    /*
    smp_ofs_t ofs_in_chk;
    Chunk_T *chk = T_allocator<Chunk_T>::alloc(), *buf_chk = get_chunk(_chk_ofs);
    smp_ofs_t to_fill = Chunk_T::chunk_size, loop_fill;
    typename Chunk_T::sample_t *to_ptr = chk->_data;
    _data_lock.acquire();
    while (to_fill > 0)
    {
        while (_smp_ofs < 0 && to_fill > 0)
        {
            (*to_ptr)[0] = 0.0f;
            (*to_ptr++)[1] = 0.0f;
            ++_smp_ofs;
            //--loop_fill;
            --to_fill;
        }
        if (to_fill == 0)
            break;
        ofs_in_chk = _smp_ofs % Chunk_T::chunk_size;
        loop_fill = std::min(to_fill, Chunk_T::chunk_size - ofs_in_chk);
        memcpy(to_ptr, buf_chk->_data + ofs_in_chk, loop_fill*sizeof(typename Chunk_T::sample_t));
        to_fill -= loop_fill;
        to_ptr += loop_fill;
        if (ofs_in_chk + loop_fill >= Chunk_T::chunk_size)
            buf_chk = get_chunk(++_chk_ofs);
        ofs_in_chk = (ofs_in_chk + loop_fill) % Chunk_T::chunk_size;
        _smp_ofs += loop_fill;
    }
    _data_lock.release();
    return chk;*/
}
