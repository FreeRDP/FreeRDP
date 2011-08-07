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
