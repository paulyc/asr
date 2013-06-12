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

#ifndef _BEATS_H
#define _BEATS_H

#include <assert.h>
#include <dirent.h>
#include <sys/types.h>

#include "fft.h"
#include "buffer.h"
#include "wavfile.h"

template <typename T>
class LengthFindingSource : public T_sink_source<T>
{
public:
    LengthFindingSource(T_source<T> *src);
    ~LengthFindingSource();
    
    T *next();
    
    void reset_ptr()
    {
        _readIndx = 0;
    }
    
private:
    std::vector<T*> *_chunks;
    int _readIndx;
};

template <typename Chunk_T>
class BeatDetector
{
public:
	BeatDetector();
    ~BeatDetector();

	void reset_source(T_source<Chunk_T> *src, CriticalSectionGuard *lock);
    void reset_stats();
    virtual void seek_chk(int chk_ofs);
    void calc_beats();
    double analyze();
    double filter(double avg, double stddev, int count);
    void process_all_from_source(T_source<Chunk_T> *src, CriticalSectionGuard *lock=0);
    void process_chunk(Chunk_T *process_chk);
    
	Chunk_T *next()
	{
        return 0;
	}
    
	const std::vector<double>& beats()
	{
		return _beat_avg_list;
	}

	struct point
	{
		point() : valid(false), t(0.0),x(0.0), dx(0.0){}
		point(double _t, double _x, double _dx) : valid(true), t(_t), x(_x),dx(_dx){}
		bool valid;
		double t;
		double x;
		double dx;
	};
    struct new_beat
    {
        new_beat() : bpm(0.0), t(0.0){}
        new_beat(double bpm_, double t_) : bpm(bpm_), t(t_) {}
        double bpm;
        double t;
    };
    
    const static int NUM_JOBS = 4;
    
    class process_beats_job : public Worker::job
    {
    public:
        process_beats_job(const FFTWindowFilter &f1, const FFTWindowFilter &f2);
        void reset_source(T_source<Chunk_T> *_src, int nChunks, CriticalSectionGuard *lock);
        void do_it();
        void step();
        void wait_for();
        
        std::vector<Chunk_T*> _outputs;
        
    private:
        VectorSource<Chunk_T> *_vecSrc;
        STFTStream _s1, _s2;
        full_wave_rectifier<SamplePairf, Chunk_T> _rectifier;
        
        Lock_T _lock;
        Condition_T _done;
        CriticalSectionGuard *_guardLock;
    };
    
    static void test_main();

private:
    int _chk_ofs;
	std::list<point> _peak_list;
	std::list<point> _beat_list;
	std::vector<double> _beat_avg_list;

	double _t;
	bool _start, _pos;
	Differentiator _diff, _diff2;
	double _x_max;
	point _max_peak;
	point _max;
	std::list<point> _maxs;
	point _last_beat;

    LPFilter _lpf;
    BPFilter _bpf;
    KaiserWindowFilter _kf;
    
	double _dt_sum;
	int _dt_points;
	double _dt_avg;
	double _last_t;
    std::list<new_beat> _bpm_list;
    std::list<double> _final_bpm_list;
    
    bool _running;
    process_beats_job *_jobs[NUM_JOBS];
};

#include "beats.cpp"

#endif
