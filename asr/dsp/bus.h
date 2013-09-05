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

template <typename Source_T, typename Sink_T>
class io_matrix : public T_sink<typename Source_T::chunk_t>
{
public:
	io_matrix():T_sink<typename Source_T::chunk_t>(0){}
	void map(Source_T *input, Sink_T *output)
	{
		_src_set.insert(input);
		_io_map[output].insert(input);
		if (_gain_map.find(std::pair<Source_T*, Sink_T*>(input, output)) != _gain_map.end())
			set_gain(input, output, 1.0);
	}
	void set_gain(Source_T *input, Sink_T *output, double gain=1.0)
	{
		_gain_map[std::pair<Source_T*, Sink_T*>(input, output)] = gain;
	}
	void unmap_input(Source_T *input)
	{
		_src_set.erase(input);
		for (typename std::map<Sink_T*, std::set<Source_T*> >::iterator i = _io_map.begin();
			i != _io_map.end(); ++i)
		{
			i->second.erase(input);
		}
		_chk_map.erase(input);
	}
	void unmap_output(Sink_T *output)
	{
		_io_map.erase(output);
		remap_inputs();
	}
	void remap_inputs()
	{
		_chk_map.clear();
		_src_set.clear();
		for (typename std::map<Sink_T*, std::set<Source_T*> >::iterator mi = _io_map.begin(); mi != _io_map.end(); ++mi)
		{
			for (typename std::set<Source_T*>::iterator i = mi->second.begin(); i != mi->second.end(); ++i)
			{
				_src_set.insert(*i);
			}
		}
	}
	// 0 <= m <= 100
	void xfade_2(int m, Source_T *in1, Source_T *in2, Sink_T *out)
	{
		if (m == 0)
		{
			set_gain(in1, out, 1.0);
			set_gain(in2, out, 0.0);
		}
		else if (m == 50)
		{
			set_gain(in1, out, 1.0);
			set_gain(in2, out, 1.0);
		}
		else if (m < 50)
		{
			set_gain(in1, out, 1.0);
			set_gain(in2, out, pow(10.0, (-30.0 + m/1.633)/20.0));
		}
		else
		{
			set_gain(in2, out, 1.0);
			set_gain(in1, out, pow(10.0, (-30.0 + (100-m)/1.633)/20.0));
		}
	}
	void unmap(Source_T *input, Sink_T *output)
	{
		bool still_has_input = false;
		for (typename std::map<Sink_T*, std::set<Source_T*> >::iterator mi = _io_map.begin(); mi != _io_map.end(); ++mi)
		{
			bool has = false;
			for (typename std::set<Source_T*>::iterator li = mi->second.begin();
				li != mi->second.end(); ++li)
			{
				if (*li == input)
				{
					has = true;
					break;
				}
			}
			if (has)
			{
				if (mi->first == output)
					mi->second.erase(input);
				else
					still_has_input = true;
			}
		}
		if (!still_has_input)
		{
			_src_set.erase(input);
			_chk_map.erase(input);
		}
	}
	void process(bool freeme=true)
	{
		//for each input, process chunk on each output
		for (typename std::set<Source_T*>::iterator i = _src_set.begin(); i != _src_set.end(); ++i)
		{
			_chk_map[*i] = (*i)->next(this);
		}

		for (typename std::map<Sink_T*, std::set<Source_T*> >::iterator mi = _io_map.begin(); mi != _io_map.end(); ++mi)
		{
			typename Source_T::chunk_t *out = T_allocator<typename Source_T::chunk_t>::alloc();
			for (int ofs = 0; ofs < Source_T::chunk_t::chunk_size; ++ofs)
			{
				out->_data[ofs][0] = 0.0;
				out->_data[ofs][1] = 0.0;
			}
			for (typename std::set<Source_T*>::iterator si = mi->second.begin(); si != mi->second.end(); ++si)
			{
				double gain = _gain_map[std::pair<Source_T*, Sink_T*>(*si, mi->first)];
				for (int ofs = 0; ofs < Source_T::chunk_t::chunk_size; ++ofs)
				{
					out->_data[ofs][0] += _chk_map[*si]->_data[ofs][0] * gain;
					out->_data[ofs][1] += _chk_map[*si]->_data[ofs][1] * gain;
				}
			}
			mi->first->process(out);
		//	_out_q[mi->first].push_back(out);
		}

		for (typename std::set<Source_T*>::iterator i = _src_set.begin(); i != _src_set.end(); ++i)
		{
			T_allocator<typename Source_T::chunk_t>::free(_chk_map[*i]);
		}
	}
/*	typename Source_T::chunk_t* next(Sink_T *_this)
	{
		typename Source_T::chunk_t* chk = _out_q[_this].front();
		_out_q[_this].pop();
		return chk;
	}*/
protected:
	std::set<Source_T*> _src_set;
	std::map<Sink_T*, std::set<Source_T*> > _io_map;
	std::map<Source_T*, typename Source_T::chunk_t*> _chk_map;
	std::map<std::pair<Source_T*,Sink_T*>, double> _gain_map;
	//std::map<Sink_T*, std::queue<typename Source_T::chunk_t*> > _out_q;
};

template <typename Chunk_T>
class bus : public T_source<Chunk_T>, public T_sink<Chunk_T>
{
public:
	typedef Chunk_T chunk_t;
	bus(T_sink<Chunk_T> *p) :
	  _parent(p) {}
	void process(Chunk_T *chk)
	{
		for (typename std::map<T_sink<Chunk_T>*, std::queue<Chunk_T*> >::iterator i = _chks_map.begin();
			i != _chks_map.end(); ++i)
		{
			i->second.push(chk);
			T_allocator<Chunk_T>::add_refx(chk);
		}
		T_allocator<Chunk_T>::free(chk);
	}
	void map_output(T_sink<Chunk_T> *out)
	{
		if (_chks_map.find(out) == _chks_map.end())
			_chks_map[out] = std::queue<Chunk_T*>();
	}
	void unmap_output(T_sink<Chunk_T> *out)
	{
		_chks_map.erase(out);
	}
	void unmap_outputs()
	{
		_chks_map.clear();
	}
	Chunk_T *next(T_sink<Chunk_T> *caller)
	{
		Chunk_T *chk;
		if (_chks_map[caller].empty())
			_parent->process();
		chk = _chks_map[caller].front();
		_chks_map[caller].pop();
		return chk;
	}
protected:
	T_sink<Chunk_T> *_parent;
	std::map<T_sink<Chunk_T>*, std::queue<Chunk_T*> > _chks_map;
};