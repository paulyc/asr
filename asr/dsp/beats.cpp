template <typename Chunk_T>
BeatDetector<Chunk_T>::BeatDetector() :
T_sink_source<Chunk_T>(0),
_my_src(0),
_rectifier(_s1),
_passthrough_sink(_s2),
_diff(10),
_lpf(2048, 44100.0, 100.0),
_bpf(2048, 44100.0, 20.0, 200.0),
_kf(1024),
_running(true),
_thissrc(0)
{
    _lpf.init();
    _bpf.init();
    _kf.init();
    
#if PARALLEL_PROCESS
#else
    _s1 = new STFTStream(_lpf, 2048, 1024, 20);
    _s2 = new STFTStream(_kf, 1024, 512, 20);
#endif
}

template <typename Chunk_T>
BeatDetector<Chunk_T>::~BeatDetector()
{
#if PARALLEL_PROCESS
 /*   _proc_lock.acquire();
    _running = false;
    for (int i=0; i<NUM_JOBS; ++i)
    {
        _do_proc.signal();
    }
    _proc_lock.release();
    
    void *ret;
    for (int i=0; i<NUM_JOBS; ++i)
    {
        pthread_join(_jobs[i]->thread, &ret);
        delete _jobs[i];
    }*/
#else
    delete _s1;
    delete _s2;
#endif
}

template <typename Chunk_T>
double BeatDetector<Chunk_T>::analyze()
{
    /*   double square_sum = 0.0;
     int square_count = 0;
     for (typename std::list<new_beat>::iterator i = _bpm_list.begin();
     i != _bpm_list.end();
     i++)
     {
     square_sum += i->bpm * i->bpm;
     ++square_count;
     }*/
    const double avg = 60.0/_dt_avg;
    const double stddev = avg * 0.1; //sqrt(square_sum/square_count - avg*avg);//avg * 0.1;
    //   printf("avg = %f stddev = %f\n", avg, stddev);
    
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
    calc_beats();
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
    delete _thissrc;
    _thissrc = new LengthFindingSource<Chunk_T>(src);
    
    const int chks_to_process = _thissrc->len().chunks;
    const int division_size = chks_to_process / NUM_JOBS;
    const int division_rem = chks_to_process % NUM_JOBS;
    
    process_beats_job *jobs[NUM_JOBS];
    
    for (int j=0; j<NUM_JOBS-1; ++j)
    {
        jobs[j] = new process_beats_job(_thissrc, division_size, _lpf, _kf);
        Worker::do_job(jobs[j], false, false, false);
    }
    jobs[NUM_JOBS-1] = new process_beats_job(_thissrc, division_size+division_rem, _lpf, _kf);
    Worker::do_job(jobs[NUM_JOBS-1], false, false, false);
    
    _thissrc->reset_ptr();
    
    for (int j=0; j<NUM_JOBS; ++j)
    {
        jobs[j]->wait_for();
        for (int i=0; i<jobs[j]->_outputs.size(); ++i)
            process_chunk(jobs[j]->_outputs[i]);
        delete jobs[j];
    }
    
    std::cout << analyze() << std::endl;
}

template <typename Chunk_T>
void BeatDetector<Chunk_T>::process_chunk(Chunk_T *process_chk)
{
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
                //	printf("hello\n");
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
