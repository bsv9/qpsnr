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

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <string>
#include <iostream>

#define	LEVEL_LOG_ERROR		(0x01)
#define	LEVEL_LOG_WARNING	(0x02)
#define	LEVEL_LOG_INFO		(0x04)
#define	LEVEL_LOG_DEBUG		(0x08)

#define	__LOG(x)		if ( x & settings::LOG) std::cerr

#define LOG_ERROR 	__LOG(LEVEL_LOG_ERROR) << "[ERROR] "
#define LOG_WARNING	__LOG(LEVEL_LOG_WARNING) << "[WARNING] "
#define LOG_INFO 	__LOG(LEVEL_LOG_INFO) << "[INFO] "
#define LOG_DEBUG 	__LOG(LEVEL_LOG_DEBUG) << "[DEBUG] "

namespace settings {
	extern char		LOG;
	extern std::string	REF_VIDEO;
	extern int		MAX_FRAMES;
	extern int		SKIP_FRAMES;
	extern bool		SAVE_IMAGES;
	extern std::string	ANALYZER;
	extern bool		IGNORE_FPS;
	extern int		VIDEO_SIZE_W;
	extern int		VIDEO_SIZE_H;
}


#endif /*_SETTINGS_H_*/

