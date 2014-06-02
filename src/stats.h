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

#ifndef _STATS_H_
#define _STATS_H_

#include <vector>
#include <ostream>
#include <string>

namespace stats {
	typedef std::vector<unsigned char>	VUCHAR;

	class s_base {
	protected:
		const int	_n_streams,
				_i_width,
				_i_height;
		std::ostream	&_ostr;
	public:
		s_base(const int& n_streams, const int& i_width, const int& i_height, std::ostream& ostr) : 
		_n_streams(n_streams), _i_width(i_width), _i_height(i_height), _ostr(ostr) {
		}

		virtual void set_parameter(const std::string& p_name, const std::string& p_value) = 0;

		virtual void process(const int& ref_frame, VUCHAR& ref, const std::vector<bool>& v_ok, std::vector<VUCHAR>& streams) = 0;

		virtual ~s_base() {
		}
	};

	extern s_base* get_analyzer(const char* id, const int& n_streams, const int& i_width, const int& i_height, std::ostream& ostr);
}

#endif /*_STATS_H_*/

