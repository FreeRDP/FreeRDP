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

#include "bitmap.h"

/*
   RLE Compressed Bitmap Stream (RLE_BITMAP_STREAM)
   http://msdn.microsoft.com/en-us/library/cc240895%28v=prot.10%29.aspx
   pseudo-code
   http://msdn.microsoft.com/en-us/library/dd240593%28v=prot.10%29.aspx
*/

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

/**
 * Reads the supplied order header and extracts the compression
 * order code ID.
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

/**
 * Extract the run length of a compression order.
 */
static uint32 ExtractRunLength(uint32 code, uint8* pbOrderHdr, uint32* advance)
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

#define IN_UINT8_MV(_p) (*((_p)++))

/**
 * decompress a color plane
 */
static int process_plane(uint8* in, int width, int height, uint8* out, int size)
{
	int indexw;
	int indexh;
	int code;
	int collen;
	int replen;
	int color;
	int x;
	int revcode;
	uint8* last_line;
	uint8* this_line;
	uint8* org_in;
	uint8* org_out;

	org_in = in;
	org_out = out;
	last_line = 0;
	indexh = 0;
	while (indexh < height)
	{
		out = (org_out + width * height * 4) - ((indexh + 1) * width * 4);
		color = 0;
		this_line = out;
		indexw = 0;
		if (last_line == 0)
		{
			while (indexw < width)
			{
				code = IN_UINT8_MV(in);
				replen = code & 0xf;
				collen = (code >> 4) & 0xf;
				revcode = (replen << 4) | collen;
				if ((revcode <= 47) && (revcode >= 16))
				{
					replen = revcode;
					collen = 0;
				}
				while (collen > 0)
				{
					color = IN_UINT8_MV(in);
					*out = color;
					out += 4;
					indexw++;
					collen--;
				}
				while (replen > 0)
				{
					*out = color;
					out += 4;
					indexw++;
					replen--;
				}
			}
		}
		else
		{
			while (indexw < width)
			{
				code = IN_UINT8_MV(in);
				replen = code & 0xf;
				collen = (code >> 4) & 0xf;
				revcode = (replen << 4) | collen;
				if ((revcode <= 47) && (revcode >= 16))
				{
					replen = revcode;
					collen = 0;
				}
				while (collen > 0)
				{
					x = IN_UINT8_MV(in);
					if (x & 1)
					{
						x = x >> 1;
						x = x + 1;
						color = -x;
					}
					else
					{
						x = x >> 1;
						color = x;
					}
					x = last_line[indexw * 4] + color;
					*out = x;
					out += 4;
					indexw++;
					collen--;
				}
				while (replen > 0)
				{
					x = last_line[indexw * 4] + color;
					*out = x;
					out += 4;
					indexw++;
					replen--;
				}
			}
		}
		indexh++;
		last_line = this_line;
	}
	return (int) (in - org_in);
}

/**
 * 4 byte bitmap decompress
 */
static boolean bitmap_decompress4(uint8* srcData, uint8* dstData, int width, int height, int size)
{
	int code;
	int bytes_pro;
	int total_pro;

	code = IN_UINT8_MV(srcData);

	if (code != 0x10)
		return False;

	total_pro = 1;
	bytes_pro = process_plane(srcData, width, height, dstData + 3, size - total_pro);
	total_pro += bytes_pro;
	srcData += bytes_pro;
	bytes_pro = process_plane(srcData, width, height, dstData + 2, size - total_pro);
	total_pro += bytes_pro;
	srcData += bytes_pro;
	bytes_pro = process_plane(srcData, width, height, dstData + 1, size - total_pro);
	total_pro += bytes_pro;
	srcData += bytes_pro;
	bytes_pro = process_plane(srcData, width, height, dstData + 0, size - total_pro);
	total_pro += bytes_pro;

	return (size == total_pro) ? True : False;
}

/**
 * bitmap flip
 */
static int bitmap_flip(uint8* src, uint8* dst, int delta, int height)
{
	int index;

	dst = (dst + delta * height) - delta;
	for (index = 0; index < height; index++)
	{
		memcpy(dst, src, delta);
		src += delta;
		dst -= delta;
	}
	return 0;
}

/**
 * bitmap decompression routine
 */
boolean bitmap_decompress(uint8* srcData, uint8* dstData, int width, int height, int size, int srcBpp, int dstBpp)
{
	uint8* data;

	if (srcBpp == 16 && dstBpp == 16)
	{
		data = (uint8*) xmalloc(width * height * 2);
		RleDecompress16to16(srcData, size, data, width * 2, width, height);
		bitmap_flip(data, dstData, width * 2, height);
		xfree(data);
	}
	else if (srcBpp == 32 && dstBpp == 32)
	{
		if (bitmap_decompress4(srcData, dstData, width, height, size) != True)
			return False;
	}
	else if (srcBpp == 15 && dstBpp == 15)
	{
		data = (uint8*) xmalloc(width * height * 2);
		RleDecompress16to16(srcData, size, data, width * 2, width, height);
		bitmap_flip(data, dstData, width * 2, height);
		xfree(data);
	}
	else if (srcBpp == 8 && dstBpp == 8)
	{
		data = (uint8*) xmalloc(width * height);
		RleDecompress8to8(srcData, size, data, width, width, height);
		bitmap_flip(data, dstData, width, height);
		xfree(data);
	}
	else if (srcBpp == 24 && dstBpp == 24)
	{
		data = (uint8*) xmalloc(width * height * 3);
		RleDecompress24to24(srcData, size, data, width * 3, width, height);
		bitmap_flip(data, dstData, width * 3, height);
		xfree(data);
	}
	else
	{
		return False;
	}

	return True;
}
