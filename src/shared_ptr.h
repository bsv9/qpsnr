/*
*	qpsnr (C) 2010 E. Oriani, ema <AT> fastwebnet <DOT> it
*
*	This file is part of qpsnr.
*
*	qpsnr is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	qpsnr is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with qpsnr.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
*	myNZB (C) 2009 E. Oriani, ema <AT> fastwebnet <DOT> it
*
*	This file is part of myNZB.
*
*	myNZB is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	myNZB is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with myNZB.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SHARED_PTR_H_
#define _SHARED_PTR_H_

template<typename T>
class shared_ptr
{
public:
	typedef T		type;
	typedef T*		ptr_type;
	typedef unsigned int	count_type;
	
	shared_ptr() :
	_ptr(0),
	_count(0)
	{
	}

	shared_ptr(const ptr_type& in) :
	_ptr(in)
	{
		_count = new (std::nothrow) count_type;
		if (!_count)
		{
			delete in;
			throw std::bad_alloc();
		}
		*_count = 1;
	}
	
	shared_ptr(const shared_ptr& in)
	{
		_ptr = in._ptr;
		_count = in._count;
		if (_count) *_count += 1;
	}
	
	shared_ptr& operator=(const shared_ptr& in)
	{
		if (_ptr != in._ptr)
		{
			if (_ptr)
			{
				if (!--*_count)
				{
					delete _ptr;
					delete _count;
				}
			}
			_ptr = in._ptr;
			_count = in._count;
			if (_count) *_count += 1;
		}
		return *this;
	}
	
	~shared_ptr()
	{
		if (_count)
		{
			if (!--*_count)
			{
				delete _ptr;
				delete _count;
			}
		}
	}
	
	ptr_type get(void) const
	{
		return _ptr;
	}
	
	count_type get_count(void) const
	{
		if (_count) return *_count;
		return 0;
	}
	
	ptr_type operator->() const
	{
		return _ptr;
	}

	type& operator*() const
	{
		return *_ptr;
	}
	
	template<typename U>
	bool operator==(shared_ptr<U>& in)
	{
		return _ptr == in._ptr;
	}
	
	template<typename U>
	bool operator!=(shared_ptr<U>& in)
	{
		return _ptr != in._ptr;
	}
	
	template<typename U>
	bool operator<(shared_ptr<U>& in)
	{
		return _ptr < in._ptr;
	}
private:
	ptr_type	_ptr;
	count_type	*_count;
};

#endif //_SHARED_PTR_H_
