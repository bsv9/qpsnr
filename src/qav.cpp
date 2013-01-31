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

#include "qav.h"
#include "settings.h"
#include <stdexcept>
#include <sstream>

qav::qvideo::qvideo(const char* file, int _out_width, int _out_height) : frnum(0), videoStream(-1), out_width(_out_width), out_height(_out_height) {
	const char* pslash = strrchr(file, '/');
	if (pslash) fname = pslash+1;
	else fname = file;
	if (av_open_input_file(&pFormatCtx, file, NULL, 0, NULL)!=0)
		throw std::runtime_error("Can't open file");
    	if (av_find_stream_info(pFormatCtx)<0) {
		av_close_input_file(pFormatCtx);
		throw std::runtime_error("Multimedia type not supported");
	}
	LOG_INFO << "File info for (" << file << ")..." << std::endl;
	dump_format(pFormatCtx, 0, file, false);
	// find video stream (first)
	for (int i=0; i<pFormatCtx->nb_streams; i++)
        	if (CODEC_TYPE_VIDEO == pFormatCtx->streams[i]->codec->codec_type) {
        		videoStream=i;
            		break;
        	}
    	if (-1==videoStream) {
		av_close_input_file(pFormatCtx);
		throw std::runtime_error("Can't find video stream");
	}
	// Get a pointer to the codec context for the video stream
   	pCodecCtx=pFormatCtx->streams[videoStream]->codec;
    	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    	if(!pCodec) {
		av_close_input_file(pFormatCtx);
		throw std::runtime_error("Can't find codec for video stream");
	}
    	if(avcodec_open(pCodecCtx, pCodec)<0) {
		av_close_input_file(pFormatCtx);
		throw std::runtime_error("Can't open codec for video stream");
	}
	// alloacate data to extract frames
	pFrame = avcodec_alloc_frame();
	if (!pFrame) {
		avcodec_close(pCodecCtx);
    		av_close_input_file(pFormatCtx);
		throw std::runtime_error("Can't allocated frame for video stream");
	}
	// populate the out_width/out_height members
	if (out_width > 0 && out_height > 0) {
		LOG_INFO << "Output frame size for (" << file << ") is: " << out_width << 'x' << out_height << std::endl;
	} else if (-1 == out_width && -1 == out_height) {
		out_width = pCodecCtx->width;
		out_height = pCodecCtx->height;
		LOG_INFO << "Output frame size for (" << file << ") (default) is: " << out_width << 'x' << out_height << std::endl;
	} else {
		avcodec_close(pCodecCtx);
    		av_close_input_file(pFormatCtx);
		throw std::runtime_error("Invalid output frame size for video stream");
	}
	// just report if we're using a different video size
	if (out_width!=pCodecCtx->width || out_height!=pCodecCtx->height)
		LOG_WARNING << "Video (" << file <<") will get scaled: " << pCodecCtx->width << 'x' << pCodecCtx->height << " (in), " << out_width << 'x' << out_height << " (out)" << std::endl;
	// sw context
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
        				out_width, out_height, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	if (!img_convert_ctx) {
		av_free(pFrame);
		avcodec_close(pCodecCtx);
    		av_close_input_file(pFormatCtx);
		throw std::runtime_error("Can't allocated sw_scale context");
	}
}

qav::scr_size qav::qvideo::get_size(void) const {
	return scr_size(out_width, out_height);
}

int qav::qvideo::get_fps_k(void) const {
	if (pFormatCtx->streams[videoStream]->r_frame_rate.den)
		return 1000*pFormatCtx->streams[videoStream]->r_frame_rate.num/pFormatCtx->streams[videoStream]->r_frame_rate.den;
	return 0;
}

bool qav::qvideo::get_frame(std::vector<unsigned char>& out, int *_frnum) {
	out.resize(avpicture_get_size(PIX_FMT_RGB24, out_width, out_height));
	AVPacket	packet;
	bool		is_read = false;
	av_init_packet(&packet);
	while (av_read_frame(pFormatCtx, &packet)>=0) {
		if (packet.stream_index==videoStream) {
			int frameFinished = 0;
			// Decode video frame
			avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, packet.data, packet.size);
			//avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet); // packet.data, packet.size);
			if(frameFinished) {
				AVPicture picRGB;
				// Assign appropriate parts of buffer to image planes in pFrameRGB
				avpicture_fill((AVPicture*)&picRGB, (unsigned char*)&out[0], PIX_FMT_RGB24, out_width, out_height);
                		// Convert the image from its native format to RGB
                		//img_convert((AVPicture*)&picRGB, PIX_FMT_RGB24, (AVPicture*)pFrame, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
       				sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, picRGB.data, picRGB.linesize);
				++frnum;
				if (_frnum) *_frnum = frnum;
				if (settings::SAVE_IMAGES) {
					if ((settings::SKIP_FRAMES < frnum) && (-1 == settings::MAX_FRAMES || frnum <= settings::MAX_FRAMES))
						save_frame(&out[0]);
				}
				is_read=true;
			}
		}
		av_free_packet(&packet);
		if (is_read) return true;
	}	
	return false;
}

/*bool SaveTGA(char *name, const unsigned char *data, int sizeX, int sizeY) {
	BYTE	TGAheader[12]={0,0,2,0,0,0,0,0,0,0,0,0};
	BYTE	header[6];
	header[1] = (BYTE)(sizeX/256);
	header[0] = (BYTE) sizeX%256;
	header[3] = (BYTE)(sizeY/256);
	header[2] = (BYTE) sizeY%256;
	header[4] = (BYTE) 24;
	header[5] = (BYTE) 0x00;
	int fh = open(name, _O_CREAT|_O_TRUNC|_O_WRONLY);
	if (-1 == fh) return false;
	if (12*sizeof(BYTE) != write(fh, TGAheader, 12*sizeof(BYTE))) return false;
	if (6*sizeof(BYTE) != write(fh, header, 6*sizeof(BYTE))) return false;
	for (int i = 0; i < sizeY; i++) {
		if (3*sizeX != write(fh, data + sizeX*i*3, sizeX*3)) {
			close(fh);
			return false;
		}
	}
	close(fh);
	return true;
}*/

void qav::qvideo::save_frame(const unsigned char *buf, const char* __fname) {
	FILE 		*pFile;
	std::string	s_fname;
	char		num_buf[32];

	//
	sprintf(num_buf, ".%08d.ppm", frnum);
	num_buf[31] = '\0';
	std::ostringstream oss;
	oss << ((__fname) ? __fname : fname.c_str()) << num_buf;
	// Open file
	pFile=fopen(oss.str().c_str(), "wb");
	if(pFile==NULL)
		return;

    	// Write header
    	fprintf(pFile, "P6\n%d %d\n255\n", out_width, out_height);

    	// Write pixel data
    	for(int y=0; y<out_height; y++)
        	fwrite(buf+y*out_width*3, 1, out_width*3, pFile);

    	// Close file
    	fclose(pFile);
}

qav::qvideo::~qvideo() {
	sws_freeContext(img_convert_ctx);
	av_free(pFrame);
	avcodec_close(pCodecCtx);
	av_close_input_file(pFormatCtx);
}

