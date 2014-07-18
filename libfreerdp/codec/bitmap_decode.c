/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Bitmap Decompression
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/stream.h>

#include "planar.h"

#include <freerdp/codec/color.h>

#include <freerdp/codec/bitmap.h>

/*
   RLE Compressed Bitmap Stream (RLE_BITMAP_STREAM)
   http://msdn.microsoft.com/en-us/library/cc240895%28v=prot.10%29.aspx
   pseudo-code
   http://msdn.microsoft.com/en-us/library/dd240593%28v=prot.10%29.aspx
*/

#define REGULAR_BG_RUN              0x00
#define MEGA_MEGA_BG_RUN            0xF0
#define REGULAR_FG_RUN              0x01
#define MEGA_MEGA_FG_RUN            0xF1
#define LITE_SET_FG_FG_RUN          0x0C
#define MEGA_MEGA_SET_FG_RUN        0xF6
#define LITE_DITHERED_RUN           0x0E
#define MEGA_MEGA_DITHERED_RUN      0xF8
#define REGULAR_COLOR_RUN           0x03
#define MEGA_MEGA_COLOR_RUN         0xF3
#define REGULAR_FGBG_IMAGE          0x02
#define MEGA_MEGA_FGBG_IMAGE        0xF2
#define LITE_SET_FG_FGBG_IMAGE      0x0D
#define MEGA_MEGA_SET_FGBG_IMAGE    0xF7
#define REGULAR_COLOR_IMAGE         0x04
#define MEGA_MEGA_COLOR_IMAGE       0xF4
#define SPECIAL_FGBG_1              0xF9
#define SPECIAL_FGBG_2              0xFA
#define SPECIAL_WHITE               0xFD
#define SPECIAL_BLACK               0xFE

#define BLACK_PIXEL 0x000000
#define WHITE_PIXEL 0xFFFFFF

typedef UINT32 PIXEL;

static const BYTE g_MaskBit0 = 0x01; /* Least significant bit */
static const BYTE g_MaskBit1 = 0x02;
static const BYTE g_MaskBit2 = 0x04;
static const BYTE g_MaskBit3 = 0x08;
static const BYTE g_MaskBit4 = 0x10;
static const BYTE g_MaskBit5 = 0x20;
static const BYTE g_MaskBit6 = 0x40;
static const BYTE g_MaskBit7 = 0x80; /* Most significant bit */

static const BYTE g_MaskSpecialFgBg1 = 0x03;
static const BYTE g_MaskSpecialFgBg2 = 0x05;

static const BYTE g_MaskRegularRunLength = 0x1F;
static const BYTE g_MaskLiteRunLength = 0x0F;

/**
 * Reads the supplied order header and extracts the compression
 * order code ID.
 */
static INLINE UINT32 ExtractCodeId(BYTE bOrderHdr)
{
	if ((bOrderHdr & 0xC0U) != 0xC0U) {
		/* REGULAR orders
		 * (000x xxxx, 001x xxxx, 010x xxxx, 011x xxxx, 100x xxxx)
		 */
		return bOrderHdr >> 5;
	}
	else if ((bOrderHdr & 0xF0U) == 0xF0U) {
		/* MEGA and SPECIAL orders (0xF*) */
		return bOrderHdr;
	}
	else {
		/* LITE orders
		 * 1100 xxxx, 1101 xxxx, 1110 xxxx)
		 */
		return bOrderHdr >> 4;
	}
}

/**
 * Extract the run length of a compression order.
 */
static INLINE UINT32 ExtractRunLength(UINT32 code, BYTE* pbOrderHdr, UINT32* advance)
{
	UINT32 runLength;
	UINT32 ladvance;

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
			runLength = ((UINT16) pbOrderHdr[1]) | ((UINT16) (pbOrderHdr[2] << 8));
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
#define DESTWRITEPIXEL(_buf, _pix) (_buf)[0] = (BYTE)(_pix)
#define DESTREADPIXEL(_pix, _buf) _pix = (_buf)[0]
#define SRCREADPIXEL(_pix, _buf) _pix = (_buf)[0]
#define DESTNEXTPIXEL(_buf) _buf += 1
#define SRCNEXTPIXEL(_buf) _buf += 1
#define WRITEFGBGIMAGE WriteFgBgImage8to8
#define WRITEFIRSTLINEFGBGIMAGE WriteFirstLineFgBgImage8to8
#define RLEDECOMPRESS RleDecompress8to8
#define RLEEXTRA
#include "include/bitmap.c"

#undef DESTWRITEPIXEL
#undef DESTREADPIXEL
#undef SRCREADPIXEL
#undef DESTNEXTPIXEL
#undef SRCNEXTPIXEL
#undef WRITEFGBGIMAGE
#undef WRITEFIRSTLINEFGBGIMAGE
#undef RLEDECOMPRESS
#undef RLEEXTRA
#define DESTWRITEPIXEL(_buf, _pix) ((UINT16*)(_buf))[0] = (UINT16)(_pix)
#define DESTREADPIXEL(_pix, _buf) _pix = ((UINT16*)(_buf))[0]
#define SRCREADPIXEL(_pix, _buf) _pix = ((UINT16*)(_buf))[0]
#define DESTNEXTPIXEL(_buf) _buf += 2
#define SRCNEXTPIXEL(_buf) _buf += 2
#define WRITEFGBGIMAGE WriteFgBgImage16to16
#define WRITEFIRSTLINEFGBGIMAGE WriteFirstLineFgBgImage16to16
#define RLEDECOMPRESS RleDecompress16to16
#define RLEEXTRA
#include "include/bitmap.c"

#undef DESTWRITEPIXEL
#undef DESTREADPIXEL
#undef SRCREADPIXEL
#undef DESTNEXTPIXEL
#undef SRCNEXTPIXEL
#undef WRITEFGBGIMAGE
#undef WRITEFIRSTLINEFGBGIMAGE
#undef RLEDECOMPRESS
#undef RLEEXTRA
#define DESTWRITEPIXEL(_buf, _pix) do { (_buf)[0] = (BYTE)(_pix);  \
  (_buf)[1] = (BYTE)((_pix) >> 8); (_buf)[2] = (BYTE)((_pix) >> 16); } while (0)
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
#include "include/bitmap.c"

/**
 * bitmap decompression routine
 */
BOOL bitmap_decompress(BYTE* srcData, BYTE* dstData, int width, int height, int size, int srcBpp, int dstBpp)
{
	int status;
	BYTE* TmpBfr;
	BYTE* pDstData;

	if (srcBpp == 16 && dstBpp == 16)
	{
		TmpBfr = (BYTE*) _aligned_malloc(width * height * 2, 16);
		RleDecompress16to16(srcData, size, TmpBfr, width * 2, width, height);
		freerdp_bitmap_flip(TmpBfr, dstData, width * 2, height);
		_aligned_free(TmpBfr);
	}
	else if (srcBpp == 32 && dstBpp == 32)
	{
		pDstData = dstData;

		status = planar_decompress(NULL, srcData, size, &pDstData,
				PIXEL_FORMAT_XRGB32_VF, width * 4, 0, 0, width, height);

		if (status < 0)
			return FALSE;
	}
	else if (srcBpp == 15 && dstBpp == 15)
	{
		TmpBfr = (BYTE*) _aligned_malloc(width * height * 2, 16);
		RleDecompress16to16(srcData, size, TmpBfr, width * 2, width, height);
		freerdp_bitmap_flip(TmpBfr, dstData, width * 2, height);
		_aligned_free(TmpBfr);
	}
	else if (srcBpp == 8 && dstBpp == 8)
	{
		TmpBfr = (BYTE*) _aligned_malloc(width * height, 16);
		RleDecompress8to8(srcData, size, TmpBfr, width, width, height);
		freerdp_bitmap_flip(TmpBfr, dstData, width, height);
		_aligned_free(TmpBfr);
	}
	else if (srcBpp == 24 && dstBpp == 24)
	{
		TmpBfr = (BYTE*) _aligned_malloc(width * height * 3, 16);
		RleDecompress24to24(srcData, size, TmpBfr, width * 3, width, height);
		freerdp_bitmap_flip(TmpBfr, dstData, width * 3, height);
		_aligned_free(TmpBfr);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}
