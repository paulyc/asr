template <typename Source_T>
class gain : public T_source<typename Source_T::chunk_t>, public T_sink<typename Source_T::chunk_t>
{
public:
	gain(Source_T *src) :
		T_sink<Source_T>(src),
		_gain(1.0)
		{
		}

	void set_gain(double g)
	{
		_gain = g;
	}

	void set_gain_db(double g_db)
	{//wrong, inverse
		dBm<double>::calc(_gain, g_db);
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