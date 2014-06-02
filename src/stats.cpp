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

#include "stats.h"
#include "mt.h"
#include "shared_ptr.h"
#include "settings.h"
#include <cmath>
#include <string>
#include <stdexcept>
#include <set>
#include <fstream>
#include <algorithm>
#include <cstdlib>

// define these classes just locally
namespace stats {
	static double compute_psnr(const unsigned char *ref, const unsigned char *cmp, const unsigned int& sz) {
		double mse = 0.0;
		for(unsigned int i = 0; i < sz; ++i) {
			const int	diff = ref[i]-cmp[i];
			mse += (diff*diff);
		}
		mse /= (double)sz;
		if (0.0 == mse) mse = 1e-10;
		return 10.0*log10(65025.0/mse);
	}

	static double compute_ssim(const unsigned char *ref, const unsigned char *cmp, const unsigned int& x, const unsigned int& y, const unsigned int& b_sz) {
		// we return the average of all the blocks
		const unsigned int	x_bl_num = x/b_sz,
					y_bl_num = y/b_sz;
		if (!x_bl_num || !y_bl_num) return 0.0;
		std::vector<double>	ssim_accum;
		// for each block do it
		for(unsigned int yB = 0; yB < y_bl_num; ++yB)
			for(unsigned int xB = 0; xB < x_bl_num; ++xB) {
				const unsigned int	base_offset = xB*b_sz + yB*b_sz*x;
				double			ref_acc = 0.0,
							ref_acc_2 = 0.0,
							cmp_acc = 0.0,
							cmp_acc_2 = 0.0,
							ref_cmp_acc = 0.0;
				for(unsigned int j = 0; j < b_sz; ++j)
					for(unsigned int i = 0; i < b_sz; ++i) {
						// we have to multiply by 3, colorplanes are Y Cb Cr, we need
						// only Y component
						const unsigned char	c_ref = ref[3*(base_offset + j*x + i)],
									c_cmp = cmp[3*(base_offset + j*x + i)];
						ref_acc += c_ref;
						ref_acc_2 += (c_ref*c_ref);
						cmp_acc += c_cmp;
						cmp_acc_2 += (c_cmp*c_cmp);
						ref_cmp_acc += (c_ref*c_cmp);
					}
				// now finally get the ssim for this block
				// http://en.wikipedia.org/wiki/SSIM
				// http://en.wikipedia.org/wiki/Variance
				// http://en.wikipedia.org/wiki/Covariance
				const double	n_samples = (b_sz*b_sz),
						ref_avg = ref_acc/n_samples,
						ref_var = ref_acc_2/n_samples - (ref_avg*ref_avg),
						cmp_avg = cmp_acc/n_samples,
						cmp_var = cmp_acc_2/n_samples - (cmp_avg*cmp_avg),
						ref_cmp_cov = ref_cmp_acc/n_samples - (ref_avg*cmp_avg),
						c1 = 6.5025, // (0.01*255.0)^2
						c2 = 58.5225, // (0.03*255)^2
						ssim_num = (2.0*ref_avg*cmp_avg + c1)*(2.0*ref_cmp_cov + c2),
						ssim_den = (ref_avg*ref_avg + cmp_avg*cmp_avg + c1)*(ref_var + cmp_var + c2),
						ssim = ssim_num/ssim_den;
				ssim_accum.push_back(ssim);
			}

		double	avg = 0.0;
		for(std::vector<double>::const_iterator it = ssim_accum.begin(); it != ssim_accum.end(); ++it)
			avg += *it;
		return avg/ssim_accum.size();
	}

	static inline double r_0_1(const double& d) {
		return std::max(0.0, std::min(1.0, d));
	}

	static void rgb_2_hsi(unsigned char *p, const int& sz) {
		/*
		I = (1/3) *(R+G+B)
		S = 1 - ( (3/(R+G+B)) * min(R,G,B))
		H = cos^-1( ((1/2) * ((R-G) + (R - B))) / ((R-G)^2 + (R-B)*(G-B)) ^(1/2) )
		H = cos^-1 ( (((R-G)+(R-B))/2)/ (sqrt((R-G)^2 + (R-B)*(G-B) )))
		*/
		const static double PI = 3.14159265;
		for(int j =0; j < sz; j += 3) {
			const double 	r = p[j+0]/255.0,
					g = p[j+1]/255.0,
					b = p[j+2]/255.0,
					i = r_0_1((r+g+b)/3),
					s = r_0_1(1.0 - (3.0/(r+g+b) * std::min(r, std::min(g, b)))),
					h = r_0_1(acos(0.5*(r-g + r-b) / sqrt((r-g)*(r-g) + (r-b)*(g-b)))/PI);
			p[j+0] = 255.0*h + 0.5;
			p[j+1] = 255.0*s + 0.5;
			p[j+2] = 255.0*i + 0.5;
		}
	}

	static void rgb_2_YCbCr(unsigned char *p, const int& sz) {
		// http://en.wikipedia.org/wiki/YCbCr
		for(int j =0; j < sz; j += 3) {
			const double 	r = p[j+0]/255.0,
					g = p[j+1]/255.0,
					b = p[j+2]/255.0;
			p[j+0] = 16 + (65.481*r + 128.553*g + 24.966*b);
			p[j+1] = 128 + (-37.797*r - 74.203*g + 112.0*b);
			p[j+2] = 128 + (112.0*r - 93.786*g - 12.214*b);
		}
	}

	static void rgb_2_Y(unsigned char *p, const int& sz) {
		// http://en.wikipedia.org/wiki/YCbCr
		for(int j =0; j < sz; j += 3) {
			const double 	r = p[j+0]/255.0,
					g = p[j+1]/255.0,
					b = p[j+2]/255.0;
			p[j+0] = 16 + (65.481*r + 128.553*g + 24.966*b);
			p[j+1] = 0.0;
			p[j+2] = 0.0;
		}
	}

	static int getCPUcount(void) {
    		std::ifstream		cpuinfo("/proc/cpuinfo");
    		std::string		line;
    		std::set<std::string>	IDs;
    		while (cpuinfo){
        		std::getline(cpuinfo,line);
        		if (line.empty())
            			continue;
        		if (line.find("processor") != 0)
            			continue;
        		IDs.insert(line);
    		}
		if (IDs.empty()) return 1;
    		return IDs.size();
	}

	static mt::ThreadPool	__stats_tp(getCPUcount());

	class psnr_job : public mt::ThreadPool::Job {
		const unsigned char	*_ref,
					*_cmp;
		const unsigned int	_sz;
		double			&_res;
	public:
		psnr_job(const unsigned char *ref, const unsigned char *cmp, const unsigned int& sz, double& res) :
		_ref(ref), _cmp(cmp), _sz(sz), _res(res) {
		}

		virtual void run(void) {
			_res = compute_psnr(_ref, _cmp, _sz);
		}
	};

	static void get_psnr_tp(const VUCHAR& ref, const std::vector<bool>& v_ok, const std::vector<VUCHAR>& streams, std::vector<double>& res) {
		const unsigned int 			sz = v_ok.size();
		std::vector<shared_ptr<psnr_job> >	v_jobs;
		for(unsigned int i =0; i < sz; ++i) {
			if (v_ok[i]) {
				v_jobs.push_back(new psnr_job(&ref[0], &(streams[i][0]), ref.size(), res[i]));
				__stats_tp.add(v_jobs.rbegin()->get());
			} else res[i] = 0.0;
		}
		//wait for all
		for(std::vector<shared_ptr<psnr_job> >::iterator it = v_jobs.begin(); it != v_jobs.end(); ++it) {
			(*it)->wait();
			(*it) = 0;
		}
	}

	class ssim_job : public mt::ThreadPool::Job {
		const unsigned char	*_ref,
					*_cmp;
		const unsigned int	_x,
					_y,
					_b_sz;
		double			&_res;
	public:
		ssim_job(const unsigned char *ref, const unsigned char *cmp, const unsigned int& x, const unsigned int& y, const unsigned int b_sz, double& res) :
		_ref(ref), _cmp(cmp), _x(x), _y(y), _b_sz(b_sz), _res(res) {
		}

		virtual void run(void) {
			_res = compute_ssim(_ref, _cmp, _x, _y, _b_sz);
		}
	};

	static void get_ssim_tp(const VUCHAR& ref, const std::vector<bool>& v_ok, const std::vector<VUCHAR>& streams, std::vector<double>& res, const unsigned int& x, const unsigned int& y, const unsigned int& b_sz) {
		const unsigned int 			sz = v_ok.size();
		std::vector<shared_ptr<ssim_job> >	v_jobs;
		for(unsigned int i =0; i < sz; ++i) {
			if (v_ok[i]) {
				v_jobs.push_back(new ssim_job(&ref[0], &(streams[i][0]), x, y, b_sz, res[i]));
				__stats_tp.add(v_jobs.rbegin()->get());
			} else res[i] = 0.0;
		}
		//wait for all
		for(std::vector<shared_ptr<ssim_job> >::iterator it = v_jobs.begin(); it != v_jobs.end(); ++it) {
			(*it)->wait();
			(*it) = 0;
		}
	}

	class hsi_job : public mt::ThreadPool::Job {
		unsigned char		*_buf;
		const unsigned int	_sz;
	public:
		hsi_job(unsigned char *buf, const unsigned int& sz) :
		_buf(buf), _sz(sz) {
		}

		virtual void run(void) {
			rgb_2_hsi(_buf, _sz);
		}
	};

	class YCbCr_job : public mt::ThreadPool::Job {
		unsigned char		*_buf;
		const unsigned int	_sz;
	public:
		YCbCr_job(unsigned char *buf, const unsigned int& sz) :
		_buf(buf), _sz(sz) {
		}

		virtual void run(void) {
			rgb_2_YCbCr(_buf, _sz);
		}
	};

	class Y_job : public mt::ThreadPool::Job {
		unsigned char		*_buf;
		const unsigned int	_sz;
	public:
		Y_job(unsigned char *buf, const unsigned int& sz) :
		_buf(buf), _sz(sz) {
		}

		virtual void run(void) {
			rgb_2_Y(_buf, _sz);
		}
	};

	static void rgb_2_hsi_tp(VUCHAR& ref, const std::vector<bool>& v_ok, std::vector<VUCHAR>& streams) {
		const unsigned int 			sz = v_ok.size();
		std::vector<shared_ptr<hsi_job> >	v_jobs;
		v_jobs.push_back(new hsi_job(&ref[0], ref.size()));
		__stats_tp.add(v_jobs.rbegin()->get());
		for(unsigned int i =0; i < sz; ++i) {
			if (v_ok[i]) {
				v_jobs.push_back(new hsi_job(&(streams[i][0]), streams[i].size()));
				__stats_tp.add(v_jobs.rbegin()->get());
			}
		}
		//wait for all
		for(std::vector<shared_ptr<hsi_job> >::iterator it = v_jobs.begin(); it != v_jobs.end(); ++it) {
			(*it)->wait();
			(*it) = 0;
		}
	}

	static void rgb_2_YCbCr_tp(VUCHAR& ref, const std::vector<bool>& v_ok, std::vector<VUCHAR>& streams) {
		const unsigned int 			sz = v_ok.size();
		std::vector<shared_ptr<YCbCr_job> >	v_jobs;
		v_jobs.push_back(new YCbCr_job(&ref[0], ref.size()));
		__stats_tp.add(v_jobs.rbegin()->get());
		for(unsigned int i =0; i < sz; ++i) {
			if (v_ok[i]) {
				v_jobs.push_back(new YCbCr_job(&(streams[i][0]), streams[i].size()));
				__stats_tp.add(v_jobs.rbegin()->get());
			}
		}
		//wait for all
		for(std::vector<shared_ptr<YCbCr_job> >::iterator it = v_jobs.begin(); it != v_jobs.end(); ++it) {
			(*it)->wait();
			(*it) = 0;
		}
	}

	static void rgb_2_Y_tp(VUCHAR& ref, const std::vector<bool>& v_ok, std::vector<VUCHAR>& streams) {
		const unsigned int 			sz = v_ok.size();
		std::vector<shared_ptr<Y_job> >		v_jobs;
		v_jobs.push_back(new Y_job(&ref[0], ref.size()));
		__stats_tp.add(v_jobs.rbegin()->get());
		for(unsigned int i =0; i < sz; ++i) {
			if (v_ok[i]) {
				v_jobs.push_back(new Y_job(&(streams[i][0]), streams[i].size()));
				__stats_tp.add(v_jobs.rbegin()->get());
			}
		}
		//wait for all
		for(std::vector<shared_ptr<Y_job> >::iterator it = v_jobs.begin(); it != v_jobs.end(); ++it) {
			(*it)->wait();
			(*it) = 0;
		}
	}

	class psnr : public s_base {
		std::string	_colorspace;
	protected:
		void print(const int& ref_frame, const std::vector<double>& v_res) {
			_ostr << ref_frame << ',';
			for(int i = 0; i < _n_streams; ++i)
				_ostr << v_res[i] << ',';
			_ostr << std::endl;
		}

		void process_colorspace(VUCHAR& ref, const std::vector<bool>& v_ok, std::vector<VUCHAR>& streams) {
			if (_colorspace == "hsi") {
				rgb_2_hsi_tp(ref, v_ok, streams);
			} else if (_colorspace == "ycbcr") {
				rgb_2_YCbCr_tp(ref, v_ok, streams);
			} else if (_colorspace == "y") {
				rgb_2_Y_tp(ref, v_ok, streams);
			}
		}
	public:
		psnr(const int& n_streams, const int& i_width, const int& i_height, std::ostream& ostr) :
		s_base(n_streams, i_width, i_height, ostr), _colorspace("rgb") {
		}

		virtual void set_parameter(const std::string& p_name, const std::string& p_value) {
			if (p_name == "colorspace") {
				_colorspace = p_value;
				if (_colorspace != "rgb" && _colorspace != "hsi"
				&& _colorspace != "ycbcr" && _colorspace != "y")
					throw std::runtime_error("Invalid colorspace passed to analyzer");
			}
		}

		virtual void process(const int& ref_frame, VUCHAR& ref, const std::vector<bool>& v_ok, std::vector<VUCHAR>& streams) {
			if (v_ok.size() != streams.size() || v_ok.size() != (unsigned int)_n_streams) throw std::runtime_error("Invalid data size passed to analyzer");
			// process colorspace
			process_colorspace(ref, v_ok, streams);
			//
			std::vector<double>	v_res(_n_streams);
			get_psnr_tp(ref, v_ok, streams, v_res);
			//
			print(ref_frame, v_res);
		}
	};

	class avg_psnr : public psnr {
		int			_fpa,
					_accum_f,
					_last_frame;
		std::vector<double>	_accum_v;
	protected:
		void print(const int& ref_frame) {
			// check if we have to print infor or not
			if (_accum_f < _fpa) return;
			// else print data
			for(int i=0; i < _n_streams; ++i)
				_accum_v[i] /= _accum_f;
			psnr::print(ref_frame, _accum_v);
			// clear the vector and set _accum_f to 0
			_accum_v.clear();
			_accum_v.resize(_n_streams);
			_accum_f = 0;
		}
	public:
		avg_psnr(const int& n_streams, const int& i_width, const int& i_height, std::ostream& ostr) :
		psnr(n_streams, i_width, i_height, ostr), _fpa(1), _accum_f(0), _last_frame(-1), _accum_v(n_streams) {
		}

		virtual void set_parameter(const std::string& p_name, const std::string& p_value) {
			psnr::set_parameter(p_name, p_value);
			//
			if (p_name == "fpa") {
				const int fpa = atoi(p_value.c_str());
				if (fpa > 0) _fpa = fpa;
			}
		}

		virtual void process(const int& ref_frame, VUCHAR& ref, const std::vector<bool>& v_ok, std::vector<VUCHAR>& streams) {
			if (v_ok.size() != streams.size() || v_ok.size() != (unsigned)_n_streams) throw std::runtime_error("Invalid data size passed to analyzer");
			// set last frame
			_last_frame = ref_frame;
			// process colorspace
			process_colorspace(ref, v_ok, streams);
			// compute the psnr
			std::vector<double>	v_res(_n_streams);
			get_psnr_tp(ref, v_ok, streams, v_res);
			// accumulate for each
			for(int i = 0; i < _n_streams; ++i) {
				if (v_ok[i]) {
					 _accum_v[i] += v_res[i];
				} else _accum_v[i] += 0.0;
			}
			++_accum_f;
			// try to print data
			print(ref_frame);
		}

		virtual ~avg_psnr() {
			// on exit check if we have some data
			if (_accum_f > 0) {
				_ostr << _last_frame << ',';
				for(int i = 0; i < _n_streams; ++i) {
					_ostr << _accum_v[i]/_accum_f << ',';
				}
				_ostr << std::endl;
			}
		}
	};

	class ssim : public s_base {
	protected:
		int	_blocksize;

		void print(const int& ref_frame, const std::vector<double>& v_res) {
			_ostr << ref_frame << ',';
			for(int i = 0; i < _n_streams; ++i)
				_ostr << v_res[i] << ',';
			_ostr << std::endl;
		}
	public:
		ssim(const int& n_streams, const int& i_width, const int& i_height, std::ostream& ostr) :
		s_base(n_streams, i_width, i_height, ostr), _blocksize(8) {
		}

		virtual void set_parameter(const std::string& p_name, const std::string& p_value) {
			if (p_name == "blocksize") {
				const int blocksize = atoi(p_value.c_str());
				if (blocksize > 0) _blocksize = blocksize;
			}
		}

		virtual void process(const int& ref_frame, VUCHAR& ref, const std::vector<bool>& v_ok, std::vector<VUCHAR>& streams) {
			if (v_ok.size() != streams.size() || v_ok.size() != (unsigned int)_n_streams) throw std::runtime_error("Invalid data size passed to analyzer");
			// convert to Y colorspace
			rgb_2_Y_tp(ref, v_ok, streams);
			//
			std::vector<double>	v_res(_n_streams);
			get_ssim_tp(ref, v_ok, streams, v_res, _i_width, _i_height, _blocksize);
			//
			print(ref_frame, v_res);
		}
	};

	class avg_ssim : public ssim {
		int			_fpa,
					_accum_f,
					_last_frame;
		std::vector<double>	_accum_v;
	protected:
		void print(const int& ref_frame) {
			// check if we have to print infor or not
			if (_accum_f < _fpa) return;
			// else print data
			for(int i=0; i < _n_streams; ++i)
				_accum_v[i] /= _accum_f;
			ssim::print(ref_frame, _accum_v);
			// clear the vector and set _accum_f to 0
			_accum_v.clear();
			_accum_v.resize(_n_streams);
			_accum_f = 0;
		}
	public:
		avg_ssim(const int& n_streams, const int& i_width, const int& i_height, std::ostream& ostr) :
		ssim(n_streams, i_width, i_height, ostr), _fpa(1), _accum_f(0), _last_frame(-1), _accum_v(n_streams) {
		}

		virtual void set_parameter(const std::string& p_name, const std::string& p_value) {
			ssim::set_parameter(p_name, p_value);
			//
			if (p_name == "fpa") {
				const int fpa = atoi(p_value.c_str());
				if (fpa > 0) _fpa = fpa;
			}
		}

		virtual void process(const int& ref_frame, VUCHAR& ref, const std::vector<bool>& v_ok, std::vector<VUCHAR>& streams) {
			if (v_ok.size() != streams.size() || v_ok.size() != (unsigned int)_n_streams) throw std::runtime_error("Invalid data size passed to analyzer");
			// set last frame
			_last_frame = ref_frame;
			// convert to Y colorspace
			rgb_2_Y_tp(ref, v_ok, streams);
			//
			std::vector<double>	v_res(_n_streams);
			get_ssim_tp(ref, v_ok, streams, v_res, _i_width, _i_height, _blocksize);
			// accumulate for each
			for(int i = 0; i < _n_streams; ++i) {
				if (v_ok[i]) {
					 _accum_v[i] += v_res[i];
				} else _accum_v[i] += 0.0;
			}
			++_accum_f;
			// try to print data
			print(ref_frame);
		}

		virtual ~avg_ssim() {
			// on exit check if we have some data
			if (_accum_f > 0) {
				_ostr << _last_frame << ',';
				for(int i = 0; i < _n_streams; ++i) {
					_ostr << _accum_v[i]/_accum_f << ',';
				}
				_ostr << std::endl;
			}
		}
	};
}

stats::s_base* stats::get_analyzer(const char* id, const int& n_streams, const int& i_width, const int& i_height, std::ostream& ostr) {
	const std::string	s_id(id);
	if (s_id == "psnr") return new psnr(n_streams, i_width, i_height, ostr);
	else if (s_id == "avg_psnr") return new avg_psnr(n_streams, i_width, i_height, ostr);
	else if (s_id == "ssim") return new ssim(n_streams, i_width, i_height, ostr);
	else if (s_id == "avg_ssim") return new avg_ssim(n_streams, i_width, i_height, ostr);
	throw std::runtime_error("Invalid analyzer id");
}
