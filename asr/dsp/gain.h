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

#ifndef _ASR_GAIN_H_
#define _ASR_GAIN_H_

template <typename Source_T>
class gain : public T_source<typename Source_T::chunk_t>, public T_sink<typename Source_T::chunk_t>
{
public:
	gain(Source_T *src) :
		T_sink<typename Source_T::chunk_t>(src),
		_gain(1.0)
		{
		}

	void set_gain(double g)
	{
		_gain = g;
	}

	void set_gain_db(double g_db)
	{
		_gain = pow(10.0, g_db/20.0);
	}

	typename Source_T::chunk_t *next()
	{
		typename Source_T::chunk_t *chk = this->_src->next();
		for (typename Source_T::chunk_t::sample_t *s = chk->_data, 
			*end = s + Source_T::chunk_t::chunk_size; s != end; ++s)
		{
			(*s)[0] *= _gain;
			(*s)[1] *= _gain;
		}
		return chk;
	}
protected:
	double _gain;
};

#endif
