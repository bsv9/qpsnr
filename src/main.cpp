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

#include <iostream>
#include <stdexcept>
#include <vector>
#include <memory>
#include <sstream>
#include <unistd.h>
#include <getopt.h>
#include <map>
#include "mt.h"
#include "shared_ptr.h"
#include "qav.h"
#include "settings.h"
#include "stats.h"

template<typename T>
std::string XtoS(const T& in) {
	std::ostringstream	oss;
	oss << in;
	return oss.str();
}

std::string get_filename(const std::string& in) {
	const char *pslash = strrchr(in.c_str(), '/');
	if (pslash) return std::string(pslash+1);
	return in;
}

class video_producer : public mt::Thread {
	int				&_frame;
	mt::Semaphore			&_s_prod,
					&_s_cons;
	std::vector<unsigned char>	&_buf;
	qav::qvideo			&_video;
	bool				&_exit,
					&_skip;
public:
	video_producer(int& frame, mt::Semaphore& s_prod, mt::Semaphore& s_cons, std::vector<unsigned char>& buf, qav::qvideo& video, bool& __exit, bool& __skip) : 
	_frame(frame), _s_prod(s_prod), _s_cons(s_cons), _buf(buf), _video(video), _exit(__exit), _skip(__skip) {
	}

	virtual void run(void) {
		while(!_exit) {
			if (!_video.get_frame(_buf, &_frame, _skip)) _frame = -1;
			// signal cons we're done
			_s_cons.pop();
			// wait for cons to tell us to go
			_s_prod.push();
		}
	}
};

typedef std::vector<unsigned char>	VUCHAR;
typedef shared_ptr<qav::qvideo>		SP_QVIDEO;
struct vp_data {
	mt::Semaphore	prod;
	VUCHAR		buf;
	int		frame;
	SP_QVIDEO	video;
	std::string	name;
};
typedef std::vector<shared_ptr<vp_data> >		V_VPDATA;
typedef std::vector<shared_ptr<video_producer> >	V_VPTH;

const std::string	__qpsnr__ = "qpsnr",
			__version__ = "0.2.5";

void print_help(void) {
	std::cerr <<	__qpsnr__ << " v" << __version__ << " - (C) 2010, 2011, 2012 E. Oriani - 2013 E. Oriani, Paul Caron\n"
			"Usage: " << __qpsnr__ << " [options] -r ref.video compare.video1 compare.video2 ...\n\n"
			"-r,--reference:\n\tset reference video (mandatory)\n"
			"\n-v,--video-size:\n\tset analysis video size WIDTHxHEIGHT (ie. 1280x720), default is reference video size\n"
			"\n-s,--skip-frames:\n\tskip n initial frames\n"
			"\n-m,--max-frames:\n\tset max frames to process before quit\n"
			"\n-I,--save-frames:\n\tsave frames (ppm format)\n"
			"\n-G,--ignore-fps:\n\tanalyze videos even if the expected fps are different\n"
			"\n-a,--analyzer:\n"
			"\tpsnr : execute the psnr for each frame\n"
			"\tavg_psnr : take the average of the psnr every n frames (use option \"fpa\" to set it)\n"
			"\tssim : execute the ssim (Y colorspace) on the frames divided in blocks (use option \"blocksize\" to set the size)\n"
			"\tavg_ssim : take the average of the ssim (Y colorspace) every n frames (use option \"fpa\" to set it)\n"
			"\n-o,--aopts: (specify option1=value1:option2=value2:...)\n"
			"\tfpa : set the frames per average, default 25\n"
			"\tcolorspace : set the colorspace (\"rgb\", \"hsi\", \"ycbcr\" or \"y\"), default \"rgb\"\n"
			"\tblocksize : set blocksize for ssim analysis, default 8\n"
			"\n-l,--log-level:\n"
			"\t0 : No log\n"
			"\t1 : Errors only\n"
			"\t2 : Warnings and errors\n"
			"\t3 : Info, warnings and errors (default)\n"
			"\t4 : Debug, info, warnings and errors\n"
			"\n-h,--help:\n\tprint this help and exit\n"
		 <<	std::flush;
}

int parse_options(int argc, char *argv[], std::map<std::string, std::string>& aopt) {
	aopt.clear();
	opterr = 0;
     	int 	c = 0,
		option_index = 0;

	static struct option long_options[] =
	{
		{"analyzer", required_argument, 0, 'a'},
		{"max-frames", required_argument, 0, 'm'},
		{"skip-frames", required_argument, 0, 's'},
		{"reference", required_argument, 0, 'r'},
		{"log-level", required_argument, 0, 'l'},
		{"save-frames", no_argument, 0, 'I'},
		{"video-size", required_argument, 0, 'v'},
		{"ignore-fps", no_argument, 0, 'G'},
		{"help", no_argument, 0, 'h'},
		{"aopts", required_argument, 0, 'o'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long (argc, argv, "a:l:m:o:r:s:v:hIG", long_options, &option_index)) != -1) {
		switch (c) {
			case 'a':
				settings::ANALYZER = optarg;
				break;
			case 'v':
				{
					const char *p_x = strchr(optarg, 'x');
					if (!p_x) p_x = strchr(optarg, 'X');
					if (!p_x) throw std::runtime_error("Invalid video size specified (use WIDTHxHEIGHT format, ie. 1280x720)");
					const std::string	s_x(optarg, p_x-optarg),
								s_y(p_x+1);
					if (s_x.empty() || s_y.empty())
						throw std::runtime_error("Invalid video size specified (use WIDTHxHEIGHT format, ie. 1280x720)");
					const int		i_x = atoi(s_x.c_str()),
								i_y = atoi(s_y.c_str());
					if (i_x <=0 || i_y <=0)
						throw std::runtime_error("Invalid video size specified, negative or 0 width/height");
					settings::VIDEO_SIZE_W = i_x;
					settings::VIDEO_SIZE_H = i_y;
				}
				break;
			case 's':
				{
					const int skip_frames = atoi(optarg);
					if (skip_frames > 0 ) settings::SKIP_FRAMES = skip_frames;
				}
				break;
			case 'r':
				settings::REF_VIDEO = optarg;
				break;
			case 'm':
				{
					const int max_frames = atoi(optarg);
					if (max_frames > 0 ) settings::MAX_FRAMES = max_frames;
				}
				break;
			case 'l':
				if (isdigit(optarg[0])) {
					char log_level[2];
					log_level[0] = optarg[0];
					log_level[1] = '\0';
					const int log_ilev = atoi(log_level);
					switch (log_ilev) {
						case 0:
							settings::LOG = 0x00;
							break;
						case 1:
							settings::LOG = 0x01;
							break;
						case 2:
							settings::LOG = 0x03;
							break;
						case 3:
							settings::LOG = 0x07;
							break;
						case 4:
							settings::LOG = 0x0F;
							break;
						default:
							break;
					}
				}
				break;
			case 'h':
				print_help();
				exit(0);
				break;
			case 'I':
				settings::SAVE_IMAGES = true;
				break;
			case 'G':
				settings::IGNORE_FPS = true;
				break;
			case 'o':
				{
					const char 	*p_opts = optarg,
							*p_colon = 0;
					while((p_colon = strchr(p_opts, ':'))) {
						const std::string	c_opt(p_opts, (p_colon-p_opts));
						const size_t 		p_equal = c_opt.find('=');
						if (std::string::npos != p_equal)
							aopt[c_opt.substr(0, p_equal)] = c_opt.substr(p_equal+1);
						p_opts = p_colon+1;
					}
					const std::string	c_opt(p_opts);
					const size_t 		p_equal = c_opt.find('=');
					if (std::string::npos != p_equal)
						aopt[c_opt.substr(0, p_equal)] = c_opt.substr(p_equal+1);
				}
				break;
			case '?':
             			if (strchr("almorsv", optopt)) {
					std::cerr << "Option -" << (char)optopt << " requires an argument" << std::endl;
					print_help();
					exit(1);
				} else if (isprint (optopt)) {
					std::cerr << "Option -" << (char)optopt << " is unknown" << std::endl;
					print_help();
					exit(1);
				}
				break;
			default:
				std::cerr << "Invalid option: " << c << std::endl;
				print_help();
				exit(1);
				break;
		}
	}
	// fix here the frame limit
	if (settings::SKIP_FRAMES > 0 && settings::MAX_FRAMES>0) settings::MAX_FRAMES += settings::SKIP_FRAMES;
	return optind;
}

namespace producers_utils {
	void start(video_producer& ref_vpth, V_VPTH& v_th) {
		ref_vpth.start();
		for(V_VPTH::iterator it = v_th.begin(); it != v_th.end(); ++it)
			(*it)->start();
	}

	void stop(video_producer& ref_vpth, V_VPTH& v_th) {
		ref_vpth.join();
		for(V_VPTH::iterator it = v_th.begin(); it != v_th.end(); ++it)
			(*it)->join();
	}

	void sync(mt::Semaphore& sem_cons, const int& n_v_th) {
		for(int i = 0; i < n_v_th; ++i)
			sem_cons.push();
	}
	
	void lock(mt::Semaphore& sem_cons, mt::Semaphore& ref_prod, V_VPDATA& v_data) {
		// init all the semaphores
		sem_cons.push();
		ref_prod.push();
		for(V_VPDATA::iterator it = v_data.begin(); it != v_data.end(); ++it)
			(*it)->prod.push();
	}

	void unlock(mt::Semaphore& ref_prod, V_VPDATA& v_data) {
		ref_prod.pop();
		for(V_VPDATA::iterator it = v_data.begin(); it != v_data.end(); ++it)
			(*it)->prod.pop();
	}

	bool is_frame_skip(const int& frame_num) {
		return (settings::SKIP_FRAMES > 0) && (settings::SKIP_FRAMES >= frame_num);
	}

	bool is_last_frame(const int& frame_num) {
		return (settings::MAX_FRAMES > 0) && (frame_num >= settings::MAX_FRAMES);
	}
}

int main(int argc, char *argv[]) {
	try {
		std::map<std::string, std::string>	aopt;
		const int param = parse_options(argc, argv, aopt);
		// Register all formats and codecs
		av_register_all();
		if (settings::REF_VIDEO == "")
			throw std::runtime_error("Reference video not specified");
		bool		glb_exit = false;
		mt::Semaphore	sem_cons;
		// create data for reference video
		mt::Semaphore	ref_prod;
		VUCHAR		ref_buf;
		int		ref_frame;
		qav::qvideo	ref_video(settings::REF_VIDEO.c_str(), settings::VIDEO_SIZE_W, settings::VIDEO_SIZE_H);
		// get const values
		const qav::scr_size	ref_sz = ref_video.get_size();
		const int		ref_fps_k = ref_video.get_fps_k();
		//
		//ref_video.get_frame(ref_buf);
		//return 0;
		// 
		V_VPDATA	v_data;
		for(int i = param; i < argc; ++i) {
			try {
				shared_ptr<vp_data>	vpd(new vp_data);
				vpd->name = get_filename(argv[i]);
				vpd->video = new qav::qvideo(argv[i], ref_sz.x, ref_sz.y);
				if (vpd->video->get_fps_k() != ref_fps_k) {
					if (settings::IGNORE_FPS) {
						LOG_WARNING << '[' << argv[i] << "] has different FPS (" << vpd->video->get_fps_k()/1000 << ')' << std::endl;
						v_data.push_back(vpd);
					} else LOG_ERROR << '[' << argv[i] << "] skipped different FPS" << std::endl;
				} else v_data.push_back(vpd);
			} catch(std::exception& e) {
				LOG_ERROR << '[' << argv[i] << "] skipped " << e.what() << std::endl;
			}
		}
		if (v_data.empty()) return 0;
		// print some infos
		LOG_INFO << "Skip frames: " << ((settings::SKIP_FRAMES > 0) ? settings::SKIP_FRAMES : 0) << std::endl;
		LOG_INFO << "Max frames: " << ((settings::MAX_FRAMES > 0) ? settings::MAX_FRAMES : 0) << std::endl;
		// create the stats analyzer (like the psnr)
		LOG_INFO << "Analyzer set: " << settings::ANALYZER << std::endl;
		std::auto_ptr<stats::s_base>	s_analyzer(stats::get_analyzer(settings::ANALYZER.c_str(), v_data.size(), ref_sz.x, ref_sz.y, std::cout));
		// set the default values, in case will get overwritten
		s_analyzer->set_parameter("fpa", XtoS(ref_fps_k/1000));
		s_analyzer->set_parameter("blocksize", "8");
		// load the passed parameters
		for(std::map<std::string, std::string>::const_iterator it = aopt.begin(); it != aopt.end(); ++it) {
			LOG_INFO << "Analyzer parameter: " << it->first << " = " << it->second << std::endl;
			s_analyzer->set_parameter(it->first.c_str(), it->second.c_str());
		}
		// create all the threads
		// this varibale holds a bool to say if we have to skip
		// or not the next frame to extract, first frame is 1
		bool skip_next_frame = producers_utils::is_frame_skip(1);
		video_producer	ref_vpth(ref_frame, ref_prod, sem_cons, ref_buf, ref_video, glb_exit, skip_next_frame);
		V_VPTH		v_th;
		for(V_VPDATA::iterator it = v_data.begin(); it != v_data.end(); ++it)
			v_th.push_back(new video_producer((*it)->frame, (*it)->prod, sem_cons, (*it)->buf, *((*it)->video), glb_exit, skip_next_frame));
		// we'll need some tmp buffers
		VUCHAR			t_ref_buf;
		std::vector<VUCHAR>	t_bufs(v_data.size());
		// and now the core algorithm
		// init all the semaphores
		producers_utils::lock(sem_cons, ref_prod, v_data);
		// start the threads
		producers_utils::start(ref_vpth, v_th);
		// print header, this has to be moved in the analyzer
		std::cout << "Sample,";
		for(V_VPDATA::const_iterator it = v_data.begin(); it != v_data.end(); ++it)
			std::cout << (*it)->name << ',';
		std::cout << std::endl;
		while(!glb_exit) {
			// wait for the consumer to be signalled 1 + n times
			const static int CONS_SIG_NUM = 1 + v_th.size();
			producers_utils::sync(sem_cons, CONS_SIG_NUM);
			// now check everything is ok
			const int	cur_ref_frame = ref_frame;
			if (-1 == cur_ref_frame) {
				glb_exit = true;
				// allow the producers to run
				producers_utils::unlock(ref_prod, v_data);
				continue;
			}
			skip_next_frame = producers_utils::is_frame_skip(cur_ref_frame+1);
			// set if we have to exit
			glb_exit = producers_utils::is_last_frame(cur_ref_frame);
			// in case we have to skip frames or exit...
			if (skip_next_frame || glb_exit) {
				// allow the producers to run
				producers_utils::unlock(ref_prod, v_data);
				continue;
			}
			// vector of bool telling if everything is ok
			std::vector<bool>	v_ok;
			for(V_VPDATA::const_iterator it = v_data.begin(); it != v_data.end(); ++it)
				if ((*it)->frame == cur_ref_frame) v_ok.push_back(true);
				else v_ok.push_back(false);
			// then swap the vectors
			t_ref_buf.swap(ref_buf);
			for(size_t i = 0; i < v_data.size(); ++i)
				t_bufs[i].swap(v_data[i]->buf);
			// allow the producers to run
			producers_utils::unlock(ref_prod, v_data);
			// finally process data
			if(!producers_utils::is_frame_skip(cur_ref_frame))
				s_analyzer->process(cur_ref_frame, t_ref_buf, v_ok, t_bufs);
		}
		// wait for all threads
		producers_utils::stop(ref_vpth, v_th);
	} catch(std::exception& e) {
		LOG_ERROR << e.what() << std::endl;
	} catch(...) {
		LOG_ERROR << "Unknown exception" << std::endl;
	}
}
