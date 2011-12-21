/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Color Conversion Routines
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>
#include <freerdp/utils/memory.h>

int freerdp_get_pixel(uint8 * data, int x, int y, int width, int height, int bpp)
{
	int start;
	int shift;
	uint16 *src16;
	uint32 *src32;
	int red, green, blue;

	switch (bpp)
	{
		case  1:
			width = (width + 7) / 8;
			start = (y * width) + x / 8;
			shift = x % 8;
			return (data[start] & (0x80 >> shift)) != 0;
		case 8:
			return data[y * width + x];
		case 15:
		case 16:
			src16 = (uint16*) data;
			return src16[y * width + x];
		case 24:
			data += y * width * 3;
			data += x * 3;
			red = data[0];
			green = data[1];
			blue = data[2];
			return RGB24(red, green, blue);
		case 32:
			src32 = (uint32*) data;
			return src32[y * width + x];
		default:
			break;
	}

	return 0;
}

void freerdp_set_pixel(uint8* data, int x, int y, int width, int height, int bpp, int pixel)
{
	int start;
	int shift;
	int *dst32;

	if (bpp == 1)
	{
		width = (width + 7) / 8;
		start = (y * width) + x / 8;
		shift = x % 8;
		if (pixel)
			data[start] = data[start] | (0x80 >> shift);
		else
			data[start] = data[start] & ~(0x80 >> shift);
	}
	else if (bpp == 32)
	{
		dst32 = (int*) data;
		dst32[y * width + x] = pixel;
	}
}

uint32 freerdp_color_convert_rgb(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	uint8 red = 0;
	uint8 green = 0;
	uint8 blue = 0;
	uint8 alpha = 0xFF;
	int dstColor = 0;

	switch (srcBpp)
	{
		case 32:
			if (clrconv->alpha)
			{
				GetARGB32(alpha, red, green, blue, srcColor);
			}
			else
			{
				GetBGR32(red, green, blue, srcColor);
			}
			break;
		case 24:
			GetBGR24(red, green, blue, srcColor);
			break;
		case 16:
			GetBGR16(red, green, blue, srcColor);
			break;
		case 15:
			GetBGR15(red, green, blue, srcColor);
			break;
		case 8:
			srcColor &= 0xFF;
			red = clrconv->palette->entries[srcColor].red;
			green = clrconv->palette->entries[srcColor].green;
			blue = clrconv->palette->entries[srcColor].blue;
			break;
		case 1:
			if (srcColor != 0)
			{
				red = 0xFF;
				green = 0xFF;
				blue = 0xFF;
			}
			break;
		default:
			break;
	}
	switch (dstBpp)
	{
		case 32:
			dstColor = ABGR32(alpha, red, green, blue);
			break;
		case 24:
			dstColor = BGR24(red, green, blue);
			break;
		case 16:
			if(clrconv->rgb555)
			{
				dstColor = RGB15(red, green, blue);
			}
			else
			{
				dstColor = RGB16(red, green, blue);
			}
			break;
		case 15:
			dstColor = RGB15(red, green, blue);
			break;
		case 8:
			srcColor &= 0xFF;
			red = clrconv->palette->entries[srcColor].red;
			green = clrconv->palette->entries[srcColor].green;
			blue = clrconv->palette->entries[srcColor].blue;
			break;
		case 1:
			if ((red != 0) || (green != 0) || (blue != 0))
				dstColor = 1;
			break;
		default:
			break;
	}

	return dstColor;
}

uint32 freerdp_color_convert_bgr(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	uint8 red = 0;
	uint8 green = 0;
	uint8 blue = 0;
	uint8 alpha = 0xFF;
	int dstColor = 0;

	switch (srcBpp)
	{
		case 32:
			if (clrconv->alpha)
			{
				GetABGR32(alpha, red, green, blue, srcColor);
			}
			else
			{
				GetBGR32(red, green, blue, srcColor);
			}
			break;
		case 24:
			GetBGR24(red, green, blue, srcColor);
			break;
		case 16:
			GetRGB16(red, green, blue, srcColor);
			break;
		case 15:
			GetRGB15(red, green, blue, srcColor);
			break;
		case 8:
			srcColor &= 0xFF;
			red = clrconv->palette->entries[srcColor].red;
			green = clrconv->palette->entries[srcColor].green;
			blue = clrconv->palette->entries[srcColor].blue;
			break;
		case 1:
			if (srcColor != 0)
			{
				red = 0xFF;
				green = 0xFF;
				blue = 0xFF;
			}
			break;
		default:
			break;
	}
	switch (dstBpp)
	{
		case 32:
			dstColor = ABGR32(alpha, red, green, blue);
			break;
		case 24:
			dstColor = BGR24(red, green, blue);
			break;
		case 16:
			if(clrconv->rgb555)
			{
				dstColor = BGR15(red, green, blue);
			}
			else
			{
				dstColor = BGR16(red, green, blue);
			}
			break;
		case 15:
			dstColor = BGR15(red, green, blue);
			break;
		case 8:
			srcColor &= 0xFF;
			red = clrconv->palette->entries[srcColor].red;
			green = clrconv->palette->entries[srcColor].green;
			blue = clrconv->palette->entries[srcColor].blue;
			break;
		case 1:
			if ((red != 0) || (green != 0) || (blue != 0))
				dstColor = 1;
			break;
		default:
			break;
	}

	return dstColor;
}

uint32 freerdp_color_convert(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	if (clrconv->invert)
		return freerdp_color_convert_bgr(srcColor, srcBpp, dstBpp, clrconv);
	else
		return freerdp_color_convert_rgb(srcColor, srcBpp, dstBpp, clrconv);
}

uint8* freerdp_image_convert_8bpp(uint8* srcData, uint8* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	int i;
	uint8 red;
	uint8 green;
	uint8 blue;
	uint32 pixel;
	uint8 *src8;
	uint16 *dst16;
	uint32 *dst32;

	if (dstBpp == 8)
	{
		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height);

		memcpy(dstData, srcData, width * height);
		return dstData;
	}
	else if (dstBpp == 15 || (dstBpp == 16 && clrconv->rgb555))
	{
		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 2);
		
		dst16 = (uint16 *) dstData;
		for (i = width * height; i > 0; i--)
		{
			pixel = *srcData;
			srcData++;
			red = clrconv->palette->entries[pixel].red;
			green = clrconv->palette->entries[pixel].green;
			blue = clrconv->palette->entries[pixel].blue;
			pixel = (clrconv->invert) ? BGR15(red, green, blue) : RGB15(red, green, blue);
			*dst16 = pixel;
			dst16++;
		}
		return dstData;
	}
	else if (dstBpp == 16)
	{
		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 2);
		
		dst16 = (uint16 *) dstData;
		for (i = width * height; i > 0; i--)
		{
			pixel = *srcData;
			srcData++;
			red = clrconv->palette->entries[pixel].red;
			green = clrconv->palette->entries[pixel].green;
			blue = clrconv->palette->entries[pixel].blue;
			pixel = (clrconv->invert) ? BGR16(red, green, blue) : RGB16(red, green, blue);
			*dst16 = pixel;
			dst16++;
		}
		return dstData;
	}
	else if (dstBpp == 32)
	{
		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 4);
		
		src8 = (uint8*) srcData;
		dst32 = (uint32*) dstData;
		for (i = width * height; i > 0; i--)
		{
			pixel = *src8;
			src8++;
			red = clrconv->palette->entries[pixel].red;
			green = clrconv->palette->entries[pixel].green;
			blue = clrconv->palette->entries[pixel].blue;
			pixel = (clrconv->invert) ? RGB32(red, green, blue) : BGR32(red, green, blue);
			*dst32 = pixel;
			dst32++;
		}
		return dstData;
	}

	return srcData;
}

uint8* freerdp_image_convert_15bpp(uint8* srcData, uint8* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	int i;
	uint8 red;
	uint8 green;
	uint8 blue;
	uint32 pixel;
	uint16 *src16;
	uint16 *dst16;
	uint32 *dst32;

	if (dstBpp == 15 || (dstBpp == 16 && clrconv->rgb555))
	{
		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 2);

		memcpy(dstData, srcData, width * height * 2);

		return dstData;
	}
	else if (dstBpp == 32)
	{
		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 4);
		
		src16 = (uint16 *) srcData;
		dst32 = (uint32 *) dstData;
		for (i = width * height; i > 0; i--)
		{
			pixel = *src16;
			src16++;
			GetBGR15(red, green, blue, pixel);
			pixel = (clrconv->invert) ? RGB32(red, green, blue) : BGR32(red, green, blue);
			*dst32 = pixel;
			dst32++;
		}
		return dstData;
	}
	else if (dstBpp == 16)
	{
		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 2);
	
		src16 = (uint16 *) srcData;
		dst16 = (uint16 *) dstData;
		for (i = width * height; i > 0; i--)
		{
			pixel = *src16;
			src16++;
			GetRGB_555(red, green, blue, pixel);
			RGB_555_565(red, green, blue);
			pixel = (clrconv->invert) ? BGR565(red, green, blue) : RGB565(red, green, blue);
			*dst16 = pixel;
			dst16++;
		}
		return dstData;
	}

	return srcData;
}

uint8* freerdp_image_convert_16bpp(uint8* srcData, uint8* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	if (srcBpp == 15)
		return freerdp_image_convert_15bpp(srcData, dstData, width, height, srcBpp, dstBpp, clrconv);

	if (dstBpp == 16)
	{
		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 2);

		if(clrconv->rgb555)
		{
			int i;
			uint8 red, green, blue;
			uint16* src16 = (uint16 *) srcData;
			uint16* dst16 = (uint16 *) dstData;

			for (i = width * height; i > 0; i--)
			{
				GetRGB_565(red, green, blue, (*src16));
				RGB_565_555(red, green, blue);
				(*dst16) = (clrconv->invert) ? BGR555(red, green, blue) : RGB555(red, green, blue);
				src16++;
				dst16++;
			}
		}
		else
		{
			memcpy(dstData, srcData, width * height * 2);
		}

		return dstData;
	}
	else if (dstBpp == 24)
	{
		int i;
		uint8 *dst8;
		uint16 *src16;
		uint8 red, green, blue;

		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 3);

		dst8 = (uint8 *) dstData;
		src16 = (uint16 *) srcData;
		for (i = width * height; i > 0; i--)
		{
			GetBGR16(red, green, blue, *src16);
			src16++;

			if (clrconv->invert)
			{
				*dst8++ = blue;
				*dst8++ = green;
				*dst8++ = red;
			}
			else
			{
				*dst8++ = red;
				*dst8++ = green;
				*dst8++ = blue;
			}
		}
		return dstData;
	}
	else if (dstBpp == 32)
	{
		int i;
		uint32 pixel;
		uint16 *src16;
		uint32 *dst32;
		uint8 red, green, blue;

		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 4);

		src16 = (uint16 *) srcData;
		dst32 = (uint32 *) dstData;
		for (i = width * height; i > 0; i--)
		{
			pixel = *src16;
			src16++;
			GetBGR16(red, green, blue, pixel);
			pixel = (clrconv->invert) ? RGB32(red, green, blue) : BGR32(red, green, blue);
			*dst32 = pixel;
			dst32++;
		}
		return dstData;
	}

	return srcData;
}

uint8* freerdp_image_convert_24bpp(uint8* srcData, uint8* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	int i;

	if (dstBpp == 32)
	{
		uint8 *dstp;
		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 4);
		
		dstp = dstData;
		for (i = width * height; i > 0; i--)
		{
			*(dstp++) = *(srcData++);
			*(dstp++) = *(srcData++);
			*(dstp++) = *(srcData++);
			*(dstp++) = 0xFF;
		}
		return dstData;
	}

	return srcData;
}

uint8* freerdp_image_convert_32bpp(uint8* srcData, uint8* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	if (dstBpp == 16)
	{
		int index;
		uint16 *dst16;
		uint32 *src32;
		uint8 red, green, blue;

		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 2);

		dst16 = (uint16*) dstData;
		src32 = (uint32*) srcData;

		for (index = 0; index < width * height; index++)
		{
			GetBGR32(blue, green, red, *src32);
			*dst16 = (clrconv->invert) ? BGR16(red, green, blue) : RGB16(red, green, blue);
			src32++;
			dst16++;
		}
		return dstData;
	}
	else if (dstBpp == 24)
	{
		uint8 *dstp;
		int index;
		uint8 red, green, blue;

		if (dstData == NULL)
			dstData = (uint8*) malloc(width * height * 3);

		dstp = dstData;
		for (index = 0; index < width * height; index++)
		{
			red = *(srcData++);
			green = *(srcData++);
			blue = *(srcData++);

			if (clrconv->invert)
			{
				*dstp++ = blue;
				*dstp++ = green;
				*dstp++ = red;
			}
			else
			{
				*dstp++ = red;
				*dstp++ = green;
				*dstp++ = blue;
			}

			srcData++;
		}
		return dstData;
	}
	else if (dstBpp == 32)
	{
		if (clrconv->alpha)
		{
			int x, y;
			uint8 *dstp;

			if (dstData == NULL)
				dstData = (uint8*) malloc(width * height * 4);

			memcpy(dstData, srcData, width * height * 4);

			dstp = dstData;
			for (y = 0; y < height; y++)
			{
				for (x = 0; x < width * 4; x += 4)
				{
					dstp += 3;
					*dstp = 0xFF;
					dstp++;
				}
			}
		}
		else
		{
			if (dstData == NULL)
				dstData = (uint8*) malloc(width * height * 4);

			memcpy(dstData, srcData, width * height * 4);
		}
		
		return dstData;
	}

	return srcData;
}

p_freerdp_image_convert freerdp_image_convert_[5] =
{
	NULL,
	freerdp_image_convert_8bpp,
	freerdp_image_convert_16bpp,
	freerdp_image_convert_24bpp,
	freerdp_image_convert_32bpp
};

uint8* freerdp_image_convert(uint8* srcData, uint8* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv)
{
	p_freerdp_image_convert _p_freerdp_image_convert = freerdp_image_convert_[IBPP(srcBpp)];

	if (_p_freerdp_image_convert != NULL)
		return _p_freerdp_image_convert(srcData, dstData, width, height, srcBpp, dstBpp, clrconv);
	else
		return 0;
}

void   freerdp_bitmap_flip(uint8 * src, uint8 * dst, int scanLineSz, int height)
{
	int i;

	uint8 * bottomLine = dst + (scanLineSz * (height - 1));
	uint8 * topLine = src;

	/* Special processing if called for flip-in-place. */
	if (src == dst)
	{
		/* Allocate a scanline buffer.
		 * (FIXME: xmalloc / xfree below should be replaced by "get/put
		 * scanline buffer from a pool/Q of fixed buffers" to reuse
		 * fixed size buffers (of max scanline size (or adaptative?) )
		 * -- would be much faster).
		 */
		uint8 * tmpBfr = xmalloc(scanLineSz);
		int half = height / 2;
		/* Flip buffer in place by line permutations through the temp
		 * scan line buffer.
		 * Not that if height has an odd number of line, we don't need
		 * to move the center scanline anyway.
		 * Also note that in place flipping takes three memcpy() calls
		 * to process two scanlines while src to distinct dest would
		 * only requires two memcpy() calls for two scanlines.
		 */
		height--;
		for (i = 0; i < half ; i++)
		{
			memcpy(tmpBfr, topLine, scanLineSz);
			memcpy(topLine, bottomLine, scanLineSz);
			memcpy(bottomLine, tmpBfr, scanLineSz);
			topLine += scanLineSz;
			bottomLine -= scanLineSz;
			height--;
		}
		xfree(tmpBfr);
	}
	/* Flip from source buffer to destination buffer. */
	else
	{

		for (i = 0; i < height; i++)
		{
			memcpy(bottomLine, topLine, scanLineSz);
			topLine += scanLineSz;
			bottomLine -= scanLineSz;
		}
	}

}

uint8* freerdp_image_flip(uint8* srcData, uint8* dstData, int width, int height, int bpp)
{
	int scanline;

	scanline = width * (bpp / 8);

	if (dstData == NULL)
		dstData = (uint8*) xmalloc(width * height * (bpp / 8));

	freerdp_bitmap_flip(srcData, dstData, scanline, height);
	return dstData;
}

uint8* freerdp_icon_convert(uint8* srcData, uint8* dstData, uint8* mask, int width, int height, int bpp, HCLRCONV clrconv)
{
	int x, y;
	int pixel;
	uint8* data;
	uint8 bmask;
	uint32 pmask;
	uint32* icon;

	pixel = 0;
	data = freerdp_image_flip(srcData, dstData, width, height, bpp);
	dstData = freerdp_image_convert(data, NULL, width, height, bpp, 32, clrconv);

	free(data);
	bmask = mask[pixel];
	icon = (uint32*) dstData;

	if (bpp < 32)
	{
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width; x++)
			{
				if (pixel % 8 == 0)
					bmask = mask[pixel / 8];
				else
					bmask <<= 1;

				pmask = (bmask & 0x80) ? 0x00000000 : 0xFF000000;

				*icon++ |= pmask;

				pixel++;
			}
		}
	}

	free(mask);

	return dstData;
}

uint8* freerdp_glyph_convert(int width, int height, uint8* data)
{
	int x, y;
	uint8 *srcp;
	uint8 *dstp;
	uint8 *dstData;
	int scanline;

	/*
	 * converts a 1-bit-per-pixel glyph to a one-byte-per-pixel glyph:
	 * this approach uses a little more memory, but provides faster
	 * means of accessing individual pixels in blitting operations
	 */

	scanline = (width + 7) / 8;
	dstData = (uint8*) malloc(width * height);
	memset(dstData, 0, width * height);
	dstp = dstData;

	for (y = 0; y < height; y++)
	{
		srcp = data + (y * scanline);

		for (x = 0; x < width; x++)
		{
			if ((*srcp & (0x80 >> (x % 8))) != 0)
				*dstp = 0xFF;
			dstp++;

			if (((x + 1) % 8 == 0) && x != 0)
				srcp++;
		}
	}

	return dstData;
}

uint8* freerdp_mono_image_convert(uint8* srcData, int width, int height, int srcBpp, int dstBpp, uint32 bgcolor, uint32 fgcolor, HCLRCONV clrconv)
{
	int index;
	uint16* dst16;
	uint32* dst32;
	uint8* dstData;
	uint8 bitMask;
	int bitIndex;
	uint8 redBg, greenBg, blueBg;
	uint8 redFg, greenFg, blueFg;

	switch (srcBpp)
	{
		case 8:
			bgcolor &= 0xFF;
			redBg = clrconv->palette->entries[bgcolor].red;
			greenBg = clrconv->palette->entries[bgcolor].green;
			blueBg = clrconv->palette->entries[bgcolor].blue;

			fgcolor &= 0xFF;
			redFg = clrconv->palette->entries[fgcolor].red;
			greenFg = clrconv->palette->entries[fgcolor].green;
			blueFg = clrconv->palette->entries[fgcolor].blue;
			break;
		case 16:
			GetRGB16(redBg, greenBg, blueBg, bgcolor);
			GetRGB16(redFg, greenFg, blueFg, fgcolor);
			break;
		case 15:
			GetRGB15(redBg, greenBg, blueBg, bgcolor);
			GetRGB15(redFg, greenFg, blueFg, fgcolor);
			break;
		default:
			GetRGB32(redBg, greenBg, blueBg, bgcolor);
			GetRGB32(redFg, greenFg, blueFg, fgcolor);
			break;
	}

	if(dstBpp == 16)
	{
		if(clrconv->rgb555)
		{
			if(srcBpp == 16)
			{
				/* convert 15-bit colors to 16-bit colors */
				RGB16_RGB15(redBg, greenBg, blueBg, bgcolor);
				RGB16_RGB15(redFg, greenFg, blueFg, fgcolor);
			}
		}
		else
		{
			if(srcBpp == 15)
			{
				/* convert 15-bit colors to 16-bit colors */
				RGB15_RGB16(redBg, greenBg, blueBg, bgcolor);
				RGB15_RGB16(redFg, greenFg, blueFg, fgcolor);
			}
		}

		dstData = (uint8*) malloc(width * height * 2);
		dst16 = (uint16*) dstData;
		for(index = height; index > 0; index--)
		{
			/* each bit encodes a pixel */
			bitMask = *srcData;
			for(bitIndex = 7; bitIndex >= 0; bitIndex--)
			{
				if((bitMask >> bitIndex) & 0x01)
				{
					*dst16 = bgcolor;
				}
				else
				{
					*dst16 = fgcolor;
				}
				dst16++;
			}
			srcData++;
		}
		return dstData;
	}
	else if(dstBpp == 32)
	{
		dstData = (uint8*) malloc(width * height * 4);
		dst32 = (uint32*) dstData;
		for(index = height; index > 0; index--)
		{
			/* each bit encodes a pixel */
			bitMask = *srcData;
			for(bitIndex = 7; bitIndex >= 0; bitIndex--)
			{
				if((bitMask >> bitIndex) & 0x01)
				{
					*dst32 = (clrconv->invert) ? BGR32(redBg, greenBg, blueBg) : RGB32(redBg, greenBg, blueBg);
				}
				else
				{
					*dst32 = (clrconv->invert) ? BGR32(redFg, greenFg, blueFg) : RGB32(redFg, greenFg, blueFg);
				}
				dst32++;
			}
			srcData++;
		}
		return dstData;
	}

	return srcData;
}

void freerdp_alpha_cursor_convert(uint8* alphaData, uint8* xorMask, uint8* andMask, int width, int height, int bpp, HCLRCONV clrconv)
{
	int xpixel;
	int apixel;
	int i, j, jj;

	for (j = 0; j < height; j++)
	{
		jj = (bpp == 1) ? j : (height - 1) - j;
		for (i = 0; i < width; i++)
		{
			xpixel = freerdp_get_pixel(xorMask, i, jj, width, height, bpp);
			xpixel = freerdp_color_convert(xpixel, bpp, 32, clrconv);
			apixel = freerdp_get_pixel(andMask, i, jj, width, height, 1);

			if (apixel != 0)
			{
				if ((xpixel & 0xffffff) == 0xffffff)
				{
					/* use pattern (not solid black) for xor area */
					xpixel = (i & 1) == (j & 1);
					xpixel = xpixel ? 0xFFFFFF : 0;
					xpixel |= 0xFF000000;
				}
				else if (xpixel == 0xFF000000)
				{
					xpixel = 0;
				}
			}

			freerdp_set_pixel(alphaData, i, j, width, height, 32, xpixel);
		}
	}
}

void freerdp_image_swap_color_order(uint8* data, int width, int height)
{
	int x, y;
	uint32* pixel;
	uint8 a, r, g, b;

	pixel = (uint32*) data;

	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			GetARGB32(a, r, g, b, *pixel);
			*pixel = ABGR32(a, r, g, b);
			pixel++;
		}
	}
}
