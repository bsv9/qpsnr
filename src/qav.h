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

#ifndef _QAV_H_
#define _QAV_H_

// libavcodec is a C library without C++ guards...
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <string>
#include <vector>

namespace qav {
	struct scr_size {
		int	x,
			y;
		scr_size(const int& _x = 0, const int& _y = 0) : x(_x), y(_y) {
		}

		inline friend bool operator==(const scr_size& lhs, const scr_size& rhs) {
			return lhs.x==rhs.x && lhs.y==rhs.y;
		}
	};

	class qvideo {
		int			frnum,
					videoStream,
					out_width,
					out_height;
		AVFormatContext		*pFormatCtx;
		AVCodecContext  	*pCodecCtx;
		AVCodec         	*pCodec;
		AVFrame			*pFrame;
		struct SwsContext	*img_convert_ctx;
		std::string		fname;
		void free_resources(void);
	public:
		qvideo(const char* file, int _out_width = -1, int _out_height = -1);
		scr_size get_size(void) const;
		int get_fps_k(void) const;
		bool get_frame(std::vector<unsigned char>& out, int *_frnum = 0, const bool skip = false);
		void save_frame(const unsigned char *buf, const char* __fname = 0);
		~qvideo();
	};
}


#endif /*_QAV_H_*/

