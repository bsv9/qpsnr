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

#include "settings.h"

namespace settings {
	char		LOG = 0x07;
	std::string	REF_VIDEO = "";
	int		MAX_FRAMES = -1;
	int		SKIP_FRAMES = -1;
	bool		SAVE_IMAGES = false;
	std::string	ANALYZER = "psnr";
	bool		IGNORE_FPS = false;
	int		VIDEO_SIZE_W = -1;
	int		VIDEO_SIZE_H = -1;
}
