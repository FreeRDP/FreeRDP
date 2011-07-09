/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RLE Compressed Bitmap Stream
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

/*
   RLE Compressed Bitmap Stream (RLE_BITMAP_STREAM)
   http://msdn.microsoft.com/en-us/library/cc240895%28v=prot.10%29.aspx
   pseudo-code
   http://msdn.microsoft.com/en-us/library/dd240593%28v=prot.10%29.aspx
*/

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#define REGULAR_BG_RUN 0x0
#define MEGA_MEGA_BG_RUN 0xF0
#define REGULAR_FG_RUN 0x1
#define MEGA_MEGA_FG_RUN 0xF1
#define LITE_SET_FG_FG_RUN 0xC
#define MEGA_MEGA_SET_FG_RUN 0xF6
#define LITE_DITHERED_RUN 0xE
#define MEGA_MEGA_DITHERED_RUN 0xF8
#define REGULAR_COLOR_RUN 0x3
#define MEGA_MEGA_COLOR_RUN 0xF3
#define REGULAR_FGBG_IMAGE 0x2
#define MEGA_MEGA_FGBG_IMAGE 0xF2
#define LITE_SET_FG_FGBG_IMAGE 0xD
#define MEGA_MEGA_SET_FGBG_IMAGE 0xF7
#define REGULAR_COLOR_IMAGE 0x4
#define MEGA_MEGA_COLOR_IMAGE 0xF4
#define SPECIAL_FGBG_1 0xF9
#define SPECIAL_FGBG_2 0xFA
#define SPECIAL_WHITE 0xFD
#define SPECIAL_BLACK 0xFE

/*
   Bitmasks
*/
static uint8 g_MaskBit0 = 0x01; /* Least significant bit */
static uint8 g_MaskBit1 = 0x02;
static uint8 g_MaskBit2 = 0x04;
static uint8 g_MaskBit3 = 0x08;
static uint8 g_MaskBit4 = 0x10;
static uint8 g_MaskBit5 = 0x20;
static uint8 g_MaskBit6 = 0x40;
static uint8 g_MaskBit7 = 0x80; /* Most significant bit */

static uint8 g_MaskRegularRunLength = 0x1F;
static uint8 g_MaskLiteRunLength = 0x0F;

static uint8 g_MaskSpecialFgBg1 = 0x03;
static uint8 g_MaskSpecialFgBg2 = 0x05;

typedef uint32 PIXEL;

#define BLACK_PIXEL 0
#define WHITE_PIXEL 0xffffff

/*
   Reads the supplied order header and extracts the compression
   order code ID.
*/
static uint32 ExtractCodeId(uint8 bOrderHdr)
{
	int code;

	switch (bOrderHdr)
	{
		case MEGA_MEGA_BG_RUN:
		case MEGA_MEGA_FG_RUN:
		case MEGA_MEGA_SET_FG_RUN:
		case MEGA_MEGA_DITHERED_RUN:
		case MEGA_MEGA_COLOR_RUN:
		case MEGA_MEGA_FGBG_IMAGE:
		case MEGA_MEGA_SET_FGBG_IMAGE:
		case MEGA_MEGA_COLOR_IMAGE:
		case SPECIAL_FGBG_1:
		case SPECIAL_FGBG_2:
		case SPECIAL_WHITE:
		case SPECIAL_BLACK:
			return bOrderHdr;
	}
	code = bOrderHdr >> 5;
	switch (code)
	{
		case REGULAR_BG_RUN:
		case REGULAR_FG_RUN:
		case REGULAR_COLOR_RUN:
		case REGULAR_FGBG_IMAGE:
		case REGULAR_COLOR_IMAGE:
			return code;
	}
	return bOrderHdr >> 4;
}

/*
   Extract the run length of a compression order.
*/
static uint32 ExtractRunLength(uint32 code, uint8 * pbOrderHdr, uint32 * advance)
{
	uint32 runLength;
	uint32 ladvance;

	ladvance = 1;
	runLength = 0;
	switch (code)
	{
		case REGULAR_FGBG_IMAGE:
			runLength = (*pbOrderHdr) & g_MaskRegularRunLength;
			if (runLength == 0)
			{
				runLength = (*(pbOrderHdr + 1)) + 1;
				ladvance += 1;
			}
			else
			{
				runLength = runLength * 8;
			}
			break;
		case LITE_SET_FG_FGBG_IMAGE:
			runLength = (*pbOrderHdr) & g_MaskLiteRunLength;
			if (runLength == 0)
			{
				runLength = (*(pbOrderHdr + 1)) + 1;
				ladvance += 1;
			}
			else
			{
				runLength = runLength * 8;
			}
			break;
		case REGULAR_BG_RUN:
		case REGULAR_FG_RUN:
		case REGULAR_COLOR_RUN:
		case REGULAR_COLOR_IMAGE:
			runLength = (*pbOrderHdr) & g_MaskRegularRunLength;
			if (runLength == 0)
			{
				/* An extended (MEGA) run. */
				runLength = (*(pbOrderHdr + 1)) + 32;
				ladvance += 1;
			}
			break;
		case LITE_SET_FG_FG_RUN:
		case LITE_DITHERED_RUN:
			runLength = (*pbOrderHdr) & g_MaskLiteRunLength;
			if (runLength == 0)
			{
				/* An extended (MEGA) run. */
				runLength = (*(pbOrderHdr + 1)) + 16;
				ladvance += 1;
			}
			break;
		case MEGA_MEGA_BG_RUN:
		case MEGA_MEGA_FG_RUN:
		case MEGA_MEGA_SET_FG_RUN:
		case MEGA_MEGA_DITHERED_RUN:
		case MEGA_MEGA_COLOR_RUN:
		case MEGA_MEGA_FGBG_IMAGE:
		case MEGA_MEGA_SET_FGBG_IMAGE:
		case MEGA_MEGA_COLOR_IMAGE:
			runLength = ((uint16) pbOrderHdr[1]) | ((uint16) (pbOrderHdr[2] << 8));
			ladvance += 2;
			break;
	}
	*advance = ladvance;
	return runLength;
}

#define UNROLL_COUNT 4
#define UNROLL(_exp) do { _exp _exp _exp _exp } while (0)

#undef DESTWRITEPIXEL
#undef DESTREADPIXEL
#undef SRCREADPIXEL
#undef DESTNEXTPIXEL
#undef SRCNEXTPIXEL
#undef WRITEFGBGIMAGE
#undef WRITEFIRSTLINEFGBGIMAGE
#undef RLEDECOMPRESS
#undef RLEEXTRA
#define DESTWRITEPIXEL(_buf, _pix) (_buf)[0] = (uint8)(_pix)
#define DESTREADPIXEL(_pix, _buf) _pix = (_buf)[0]
#define SRCREADPIXEL(_pix, _buf) _pix = (_buf)[0]
#define DESTNEXTPIXEL(_buf) _buf += 1
#define SRCNEXTPIXEL(_buf) _buf += 1
#define WRITEFGBGIMAGE WriteFgBgImage8to8
#define WRITEFIRSTLINEFGBGIMAGE WriteFirstLineFgBgImage8to8
#define RLEDECOMPRESS RleDecompress8to8
#define RLEEXTRA
#include "bitmap_inc.c"

#undef DESTWRITEPIXEL
#undef DESTREADPIXEL
#undef SRCREADPIXEL
#undef DESTNEXTPIXEL
#undef SRCNEXTPIXEL
#undef WRITEFGBGIMAGE
#undef WRITEFIRSTLINEFGBGIMAGE
#undef RLEDECOMPRESS
#undef RLEEXTRA
#define DESTWRITEPIXEL(_buf, _pix) ((uint16*)(_buf))[0] = (uint16)(_pix)
#define DESTREADPIXEL(_pix, _buf) _pix = ((uint16*)(_buf))[0]
#define SRCREADPIXEL(_pix, _buf) _pix = ((uint16*)(_buf))[0]
#define DESTNEXTPIXEL(_buf) _buf += 2
#define SRCNEXTPIXEL(_buf) _buf += 2
#define WRITEFGBGIMAGE WriteFgBgImage16to16
#define WRITEFIRSTLINEFGBGIMAGE WriteFirstLineFgBgImage16to16
#define RLEDECOMPRESS RleDecompress16to16
#define RLEEXTRA
#include "bitmap_inc.c"

#undef DESTWRITEPIXEL
#undef DESTREADPIXEL
#undef SRCREADPIXEL
#undef DESTNEXTPIXEL
#undef SRCNEXTPIXEL
#undef WRITEFGBGIMAGE
#undef WRITEFIRSTLINEFGBGIMAGE
#undef RLEDECOMPRESS
#undef RLEEXTRA
#define DESTWRITEPIXEL(_buf, _pix) do { (_buf)[0] = (uint8)(_pix);  \
  (_buf)[1] = (uint8)((_pix) >> 8); (_buf)[2] = (uint8)((_pix) >> 16); } while (0)
#define DESTREADPIXEL(_pix, _buf) _pix = (_buf)[0] | ((_buf)[1] << 8) | \
  ((_buf)[2] << 16)
#define SRCREADPIXEL(_pix, _buf) _pix = (_buf)[0] | ((_buf)[1] << 8) | \
  ((_buf)[2] << 16)
#define DESTNEXTPIXEL(_buf) _buf += 3
#define SRCNEXTPIXEL(_buf) _buf += 3
#define WRITEFGBGIMAGE WriteFgBgImage24to24
#define WRITEFIRSTLINEFGBGIMAGE WriteFirstLineFgBgImage24to24
#define RLEDECOMPRESS RleDecompress24to24
#define RLEEXTRA
#include "bitmap_inc.c"

int bitmap_decompress(void * inst, uint8 * output, int width, int height,
	uint8 * input, int size, int Bpp)
{
	switch (Bpp)
	{
		case 1:
			RleDecompress8to8(input, size, output, width, width, height);
			break;
		case 2:
			RleDecompress16to16(input, size, output, width * 2, width, height);
			break;
		case 3:
			RleDecompress24to24(input, size, output, width * 3, width, height);
			break;
		case 4:
			/* TODO */
			break;
		default:
			/* TODO */
			break;
	}
	return 0;
}
