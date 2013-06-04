// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

template <typename T>
LengthFindingSource<T>::LengthFindingSource(T_source<T> *src) : T_sink_source<T>(src), _readIndx(0)
{
    if (this->_src->len().chunks != -1)
    {
        const int chks = this->_src->len().chunks;
        _chunks = new std::vector<T*>(chks);
        for (int i=0; i<chks; ++i)
        {
            _chunks->operator[](i) = this->_src->next();
        }
    }
    else
    {
        _chunks = new std::vector<T*>;
        _chunks->reserve(40000);
        while (this->_src->len().chunks == -1)
        {
            _chunks->push_back(this->_src->next());
        }
    }
}

template <typename T>
LengthFindingSource<T>::~LengthFindingSource()
{
    for (int i=0; i<_chunks->size(); ++i)
    {
        T_allocator<T>::free(_chunks->operator[](i));
    }
    delete _chunks;
}

template <typename T>
T *LengthFindingSource<T>::next()
{
    T *chk = _chunks->operator[](_readIndx++);
    T_allocator<T>::add_ref(chk);
    return chk;
}

template <typename Chunk_T>
BeatDetector<Chunk_T>::BeatDetector() :
_diff(10),
_lpf(2048, 44100.0, 100.0),
_bpf(2048, 44100.0, 20.0, 200.0),
_kf(1024),
_running(true)
{
    _lpf.init();
    _bpf.init();
    _kf.init();
    
    for (int i=0; i<NUM_JOBS; ++i)
    {
        _jobs[i] = new process_beats_job(_lpf, _kf);
    }
}

template <typename Chunk_T>
BeatDetector<Chunk_T>::~BeatDetector()
{
    for (int i=0; i<NUM_JOBS; ++i)
    {
        delete _jobs[i];
    }
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::reset_source(T_source<Chunk_T> *src, Lock_T *lock)
{
    reset_stats();
    process_all_from_source(src, lock);
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::reset_stats()
{
    _t = 0.0;
    _pos = true;
    _start = false;
    _x_max = 10.0;
    _chk_ofs = 0;
    
    _dt_sum = 0.0;
    _dt_points = 0;
    _dt_avg = 0.0;
    
    _peak_list.clear();
    _beat_list.clear();
    _beat_avg_list.clear();
    
    _max_peak = point();
    _max = point();
    _maxs.clear();
    _last_beat = point();
    
    _bpm_list.clear();
    _final_bpm_list.clear();
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::seek_chk(int chk_ofs)
{
    if (chk_ofs == _chk_ofs)
    {
        ++_chk_ofs;
        if (_chk_ofs % 100 == 0)
        {
			//	printf("chk %d\n", _chk_ofs);
        }
    }
    else
    {
        printf("hello\n");
    }
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::calc_beats()
{
    std::list<double>::iterator i = _final_bpm_list.begin();
    double last_t = *i;
    
    while (last_t > 0.0)
    {
        last_t -= _dt_avg;
        _final_bpm_list.push_front(last_t);
    }
    i = _final_bpm_list.begin();
    last_t = *i;
    _beat_avg_list.push_back(last_t);
    i++;
    while (i != _final_bpm_list.end())
    {
        const double dt = *i - last_t;
        if (dt > 1.5*_dt_avg)
        {
            last_t += _dt_avg;
            //					printf("bpm = %f\n", 60.0/_dt_avg);
            _beat_avg_list.push_back(last_t);
            continue;
        }
        else if (dt < 0.5*_dt_avg)
        {
            i++;
            continue;
        }
        else
        {
            last_t += _dt_avg;
            double diff = 0.1 * (*i - last_t);
            last_t += diff;
            //					printf("bpm = %f\n", 60.0/(_dt_avg+diff));
            _beat_avg_list.push_back(last_t);
            i++;
        }
    }
    for (int j=0; j<50; ++j)
    {
        //				printf("bpm = %f\n", 60.0/_dt_avg);
        last_t += _dt_avg;
        _beat_avg_list.push_back(last_t);
    }
}

template <typename Chunk_T>
double BeatDetector<Chunk_T>::analyze()
{
    const double avg = 60.0/_dt_avg;
    const double stddev = avg * 0.1;
    
    double bpm = filter(avg, stddev, 1);
    
    double final_sum = 0.0;
    int final_count = 0;
    for (typename std::list<new_beat>::iterator i = _bpm_list.begin();
         i != _bpm_list.end();
         i++)
    {
        const double dt = i->bpm;
        if (fabs(dt - bpm) / bpm < 0.2)
        {
            final_sum += dt;
            ++final_count;
            _final_bpm_list.push_back(i->t);
        }
    }
    const double final_bpm = final_sum / final_count;
    //  printf("final avg %f\n", final_bpm);
    _dt_avg = 60.0 / final_bpm;
    std::cout << final_bpm << std::endl;
    return final_bpm;
}

template <typename Chunk_T>
double BeatDetector<Chunk_T>::filter(double avg, double stddev, int count)
{
    double sum = 0.0;
    double square_sum = 0.0;
    int valid_count = 0;
    for (typename std::list<new_beat>::iterator i = _bpm_list.begin();
         i != _bpm_list.end();
         i++)
    {
        const double dt = i->bpm;
        if (fabs(dt - avg) < stddev)
        {
            sum += dt;
            square_sum += dt*dt;
            ++valid_count;
        }
    }
    const double new_avg = sum / valid_count;
    const double square_avg = square_sum / valid_count;
    const double new_stddev = sqrt(square_avg - new_avg*new_avg);
    printf("filter avg = %f filter stddev = %f\n", new_avg, new_stddev);
    if (new_stddev < new_avg * 0.05 || count >= 10)
        return new_avg;
    else
        return filter(new_avg, new_stddev, count+1);
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::process_all_from_source(T_source<Chunk_T> *src, Lock_T *lock)
{
    const int chks_to_process = src->len().chunks;
    const int division_size = chks_to_process / NUM_JOBS;
    const int division_rem = chks_to_process % NUM_JOBS;
    
    for (int j=0; j<NUM_JOBS-1; ++j)
    {
        _jobs[j]->reset_source(src, division_size, lock);
        Worker::do_job(_jobs[j], false, false, false);
    }
    _jobs[NUM_JOBS-1]->reset_source(src, division_size+division_rem, lock);
    Worker::do_job(_jobs[NUM_JOBS-1], false, false, false);
    
    
    for (int j=0; j<NUM_JOBS; ++j)
    {
        _jobs[j]->wait_for();
        for (int i=0; i<_jobs[j]->_outputs.size(); ++i)
        {
          //  if (i%10==0)
            //    CRITICAL_SECTION_GUARD(lock);
            process_chunk(_jobs[j]->_outputs[i]);
        }
    }
    
    analyze();
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::process_chunk(Chunk_T *process_chk)
{
    //T_allocator<Chunk_T>::print_info(process_chk);
    for (SamplePairf *smp = process_chk->_data, *end = smp + Chunk_T::chunk_size; smp != end; ++smp)
    {
        double x = (*smp)[0];
        _diff.next(x);
        double dx = _diff.dx();
        _diff2.next(dx);
        double ddx = _diff2.dx();
        if (x > _x_max)
        {
            _x_max = x;
            _start = true;
        }
        else if (x < 0.1 * _x_max)
        {
            if (_start)
            {
                _start = false;
                // pick point with highest dx
                for (typename std::list<point>::iterator i = _peak_list.begin(); i != _peak_list.end(); i++)
                {
                    if (i->dx > _max.dx && i->x > 0.15 * _x_max)
                    {
                        _max = *i;
                        _maxs.push_back(_max);
                    }
                    else if (!_maxs.empty() && i->x < 0.05 * _x_max)//_max.x)
                    {
                        // pick first
                        if (!_last_beat.valid && _dt_points > 25)
                        {
                            _last_beat.t = _maxs.begin()->t - _dt_avg;
                            _last_beat.valid = true;
                        }
                        if (_last_beat.valid)
                        {
                            const double dt = _maxs.begin()->t - _last_beat.t;
                            const double bpm = 60.0/dt;
                            // todo revise this: pick first 5 better
                            if (bpm > 80.0 && bpm < 180.0 && (_dt_points < 25 || fabs(dt-_dt_avg) / _dt_avg < 0.1))
                            {
                                _dt_sum += dt;
                                ++_dt_points;
                                _dt_avg = _dt_sum / _dt_points;
                            }
                            _bpm_list.push_back(new_beat(bpm, _last_beat.t));
                            //    printf("bpm %f avg %f\n", bpm, 60.0/_dt_avg);
                        }
                        _last_beat = *_maxs.begin();
                        _last_t = _last_beat.t;
                        _beat_list.push_back(*_maxs.begin());
                        _max = point();
                        _maxs.clear();
                    }
                }
                _peak_list.clear();
            }
        }
        else if (x > 0.2 * _x_max)
        {
            _start = true;
        }
        
        if (dx < 0)
        {
            if (_pos)
            {
                _pos = false;
                // record max
                if (_max_peak.valid)
                {
                    _peak_list.push_back(_max_peak);
                    _max_peak = point();
                }
            }
        }
        else
        {
            if (!_pos)
                _pos = true;
            if (dx > _max_peak.dx)
            {
                _max_peak.t = _t - 2.0/44100.0;
                _max_peak.dx = dx;
                _max_peak.x = x;
                _max_peak.valid = true;
            }
        }
        _t += 1.0 / 44100.0;
    }
    //	avg[0] /= Chunk_T::chunk_size;
    //	avg[1] /= Chunk_T::chunk_size;
    //	printf("avg values of envelope %f %f\n", avg[0], avg[1]);
    
    //	T_allocator<Chunk_T>::free(passthru_chk);
    //	return process_chk;
    //    printf("xmax %f\n", _x_max);
    T_allocator<Chunk_T>::free(process_chk);
}

template <typename Chunk_T>
BeatDetector<Chunk_T>::process_beats_job::process_beats_job(const FFTWindowFilter &f1, const FFTWindowFilter &f2) :
_vecSrc(0),
_s1(f1, 2048, 1024, 20),
_s2(f2, 1024, 512, 20),
_rectifier(&_s1)
{
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::process_beats_job::reset_source(T_source<Chunk_T> *_src, int nChunks, Lock_T *lock)
{
    _lock.acquire();
    delete _vecSrc;
    _vecSrc = new VectorSource<Chunk_T>(nChunks);
    _outputs.resize(nChunks);
    _guardLock = lock;
    for (int i=0; i<nChunks; ++i)
    {
        if (i%10==0)
        {
            CRITICAL_SECTION_GUARD(_guardLock);
        }
        Chunk_T *chk = _src->next();
        _vecSrc->add(chk);
    }
    _s1.reset_source(_vecSrc);
    _s2.reset_source(&_rectifier);
    _lock.release();
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::process_beats_job::do_it()
{
    _lock.acquire();
    for (int i=0; i<_vecSrc->size(); ++i)
    {
        if (i%10==0)
        {
            CRITICAL_SECTION_GUARD(_guardLock);
        }
        _outputs[i] = _s2.next();
    }
    
    done = true;
    _done.signal();
    _lock.release();
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::process_beats_job::step()
{
    do_it();
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::process_beats_job::wait_for()
{
    _lock.acquire();
    while (!done)
    {
        _done.wait(_lock);
    }
    _lock.release();
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::test_main()
{
    T_source<chunk_t> *src = 0;
    BeatDetector<chunk_t> detector;
    std::string filenamestr;
    dirent *entry;
    
    char filename[256];
    
    const char *dir_name = "/Users/paulyc/Downloads/";
    
    //  while (FileOpenDialog::OpenSingleFile(filenamestr))
    DIR *d = opendir(dir_name);
    while ((entry = readdir(d)) != NULL)
    {
        
        strcpy(filename, dir_name);
        strcat(filename, entry->d_name);
        //   strcpy(filename, filenamestr.c_str());
        
        if (strstr(filename, ".mp3") == filename + strlen(filename) - 4)
        {
            continue;
            src = new mp3file_chunker<Chunk_T>(filename);
        }
        else if (strstr(filename, ".wav") == filename + strlen(filename) - 4)
        {
            src = new wavfile_chunker<Chunk_T>(filename);
            //src = new ifffile_chunker<Chunk_T>(filename);
        }
        else if (strstr(filename, ".flac") == filename + strlen(filename) - 5)
        {
            continue;
            src = new flacfile_chunker<Chunk_T>(filename);
        }
        else if (strstr(filename, ".aiff") == filename + strlen(filename) - 5)
        {
            src = new ifffile_chunker<Chunk_T>(filename);
        }
        else
        {
            continue;
        }
        
        //   std::cout << filename << " ";
        
#if PARALLEL_PROCESS
        detector.process_all_from_source(src, 0);
#else
        
        detector.reset_source(src);
        int chks = 0;
        while (detector.len().chunks == -1)
        {
            ++chks;
            T_allocator<chunk_t>::free(detector.next());
        }
        while (chks < detector.len().chunks)
        {
            ++chks;
            T_allocator<chunk_t>::free(detector.next());
        }
        std::cout <<  detector.analyze() << std::endl;
#endif
        delete src;
        src = 0;
        //   break;
    }
}
