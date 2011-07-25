/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Compressed Bitmap
 *
 * Copyright 2011 Jay Sorg <jay.sorg@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BITMAP_H
#define __BITMAP_H

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#define REGULAR_BG_RUN			0x00
#define MEGA_MEGA_BG_RUN		0xF0
#define REGULAR_FG_RUN			0x01
#define MEGA_MEGA_FG_RUN		0xF1
#define LITE_SET_FG_FG_RUN		0x0C
#define MEGA_MEGA_SET_FG_RUN		0xF6
#define LITE_DITHERED_RUN		0x0E
#define MEGA_MEGA_DITHERED_RUN		0xF8
#define REGULAR_COLOR_RUN		0x03
#define MEGA_MEGA_COLOR_RUN		0xF3
#define REGULAR_FGBG_IMAGE		0x02
#define MEGA_MEGA_FGBG_IMAGE		0xF2
#define LITE_SET_FG_FGBG_IMAGE		0x0D
#define MEGA_MEGA_SET_FGBG_IMAGE	0xF7
#define REGULAR_COLOR_IMAGE		0x04
#define MEGA_MEGA_COLOR_IMAGE		0xF4
#define SPECIAL_FGBG_1			0xF9
#define SPECIAL_FGBG_2			0xFA
#define SPECIAL_WHITE			0xFD
#define SPECIAL_BLACK			0xFE

#define BLACK_PIXEL 			0x000000
#define WHITE_PIXEL 			0xFFFFFF

typedef uint32 PIXEL;

boolean bitmap_decompress(uint8* srcData, uint8* dstData, int width, int height, int size, int srcBpp, int dstBpp);

#endif /* __BITMAP_H */
